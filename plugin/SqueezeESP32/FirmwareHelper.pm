package Plugins::SqueezeESP32::FirmwareHelper;

use strict;

use File::Basename qw(basename);
use File::Spec::Functions qw(catfile);
use JSON::XS::VersionOneAndTwo;

use Slim::Utils::Log;
use Slim::Utils::Prefs;

use constant FIRMWARE_POLL_INTERVAL => 3600 * (5 + rand());
use constant GITHUB_RELEASES_URI => "https://api.github.com/repos/sle118/squeezelite-esp32/releases";
use constant GITHUB_ASSET_URI => GITHUB_RELEASES_URI . "/assets/";
use constant GITHUB_DOWNLOAD_URI => "https://github.com/sle118/squeezelite-esp32/releases/download/";
use constant ESP32_STATUS_URI => "http://%s/status.json";
use constant BASE_PATH => 'plugins/SqueezeESP32/firmware/';

my $FW_DOWNLOAD_REGEX = qr{plugins/SqueezeESP32/firmware/(-99|[-a-z0-9-/.]+\.bin)(?:\?.*)?$}i;
my $FW_CUSTOM_REGEX = qr/^((?:squeezelite-esp32-)?custom\.bin)$/;
my $FW_FILENAME_REGEX = qr/^squeezelite-esp32-.*\.bin(\.tmp)?$/;
my $FW_TAG_REGEX = qr/\b(ESP32-A1S|SqueezeAmp|I2S-4MFlash)\.(16|32)\.(\d+)\.([-a-zA-Z0-9]+)\b/;

use constant MAX_FW_IMAGE_SIZE => 10 * 1024 * 1024;

my $prefs = preferences('plugin.squeezeesp32');
my $log = logger('plugin.squeezeesp32');

my $initialized;

sub init {
	my ($client) = @_;

	if (!$initialized) {
		$initialized = 1;
		Slim::Web::Pages->addRawFunction($FW_DOWNLOAD_REGEX, \&handleFirmwareDownload);
		Slim::Web::Pages->addRawFunction('plugins/SqueezeESP32/firmware/upload', \&handleFirmwareUpload);
	}

	# start checking for firmware updates
	Slim::Utils::Timers::setTimer($client, Time::HiRes::time() + 3.0 + rand(3.0), \&initFirmwareDownload);
}

sub initFirmwareDownload {
	my ($client, $cb) = @_;

	Slim::Utils::Timers::killTimers($client, \&initFirmwareDownload);

	return unless preferences('server')->get('checkVersion') || $cb;

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;
			my $content = eval { from_json( $http->content ) };

			if ($content && ref $content) {
				my $releaseInfo = getFirmwareTag($content->{version});

				if ($releaseInfo && ref $releaseInfo) {
					prefetchFirmware($releaseInfo, $cb);
				}
				else {
					$cb->() if $cb;
				}
			}
			elsif ($cb) {
				$cb->();
			}
		},
		sub {
			my ($http, $error) = @_;
			$log->error("Failed to get releases from Github: $error");

			$cb->() if $cb;
		},
		{
			timeout => 10
		}
	)->get(sprintf(ESP32_STATUS_URI, $client->ip));

	Slim::Utils::Timers::setTimer($client, Time::HiRes::time() + FIRMWARE_POLL_INTERVAL, \&initFirmwareDownload);
}

sub prefetchFirmware {
	my ($releaseInfo, $cb) = @_;

	return unless $releaseInfo;

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;
			my $content = eval { from_json( $http->content ) };

			if (!$content || !ref $content) {
				$@ && $log->error("Failed to parse response: $@");
			}

			my $regex = $releaseInfo->{model} . '\.' . $releaseInfo->{res} . '\.\d+\.' . $releaseInfo->{branch};
			my $url;
			foreach (@$content) {
				if ($_->{tag_name} =~ /$regex/ && $_->{assets} && ref $_->{assets}) {
					($url) = grep /\.bin$/, map {
						$_->{browser_download_url}
					} @{$_->{assets}};

					last if $url;
				}
			}

			my $customFwUrl = _urlFromPath('custom.bin') if $cb && -f _customFirmwareFile();

			if ( ($url && $url =~ /^https?/) || $customFwUrl ) {
				downloadFirmwareFile(sub {
					main::INFOLOG && $log->is_info && $log->info("Pre-cached firmware file: " . $_[0]);
				}, sub {
					my ($http, $error, $url, $code) = @_;
					$error ||= ($http && $http->error) || 'unknown error';
					$url   ||= ($http && $http->url) || 'no URL';

					$log->error(sprintf("Failed to get firmware image from Github: %s (%s)", $error || $http->error, $url));
				}, $url) if $url;

				$cb->($releaseInfo, _gh2lmsUrl($url), $customFwUrl) if $cb;
			}
			elsif ($cb) {
				$cb->();
			}
		},
		sub {
			my ($http, $error) = @_;
			$log->error("Failed to get releases from Github: $error");

			$cb->() if $cb;
		},
		{
			timeout => 10,
			cache => 1,
			expires => 3600
		}
	)->get(GITHUB_RELEASES_URI);
}

sub _gh2lmsUrl {
	my ($url) = @_;
	my $ghPrefix = GITHUB_DOWNLOAD_URI;
	my $baseUrl = Slim::Utils::Network::serverURL();
	$url =~ s/$ghPrefix/$baseUrl\/plugins\/SqueezeESP32\/firmware\//;
	return $url;
}

sub _urlFromPath {
	return sprintf('%s/%s%s', Slim::Utils::Network::serverURL(), BASE_PATH, basename(shift));
}

sub _customFirmwareFile {
	return catfile(scalar Slim::Utils::OSDetect::dirsFor('updates'), 'squeezelite-esp32-custom.bin');
}

sub handleFirmwareDownload {
	my ($httpClient, $response) = @_;

	my $request = $response->request;

	my $_errorDownloading = sub {
		_errorDownloading($httpClient, $response, @_);
	};

	my $path;
	if (!defined $request || !(($path) = $request->uri =~ $FW_DOWNLOAD_REGEX)) {
		return $_errorDownloading->(undef, 'Invalid request', $request->uri, 400);
	}

	# this is the magic request used on the client to figure out whether the plugin does support download proxying
	if ($path =~ /^(?:-99|-check.bin)$/) {
		$response->code(204);
		$response->header('Access-Control-Allow-Origin' => '*');

		$httpClient->send_response($response);
		return Slim::Web::HTTP::closeHTTPSocket($httpClient);
	}

	if ($path =~ $FW_CUSTOM_REGEX) {
		my $firmwareFile = _customFirmwareFile();

		if (! -f $firmwareFile) {
			main::INFOLOG && $log->is_info && $log->info("Failed to find custom firmware build: $firmwareFile");
			$response->code(404);
			$httpClient->send_response($response);
			return Slim::Web::HTTP::closeHTTPSocket($httpClient);
		}

		main::INFOLOG && $log->is_info && $log->info("Getting custom firmware build");

		$response->code(200);
		return Slim::Web::HTTP::sendStreamingFile($httpClient, $response, 'application/octet-stream', $firmwareFile, undef, 1);
	}

	main::INFOLOG && $log->is_info && $log->info("Requesting firmware from: $path");

	downloadFirmwareFile(sub {
		my $firmwareFile = shift;
		$response->code(200);
		Slim::Web::HTTP::sendStreamingFile($httpClient, $response, 'application/octet-stream', $firmwareFile, undef, 1);
	}, $_errorDownloading, GITHUB_DOWNLOAD_URI . $path);
}

sub downloadFirmwareFile {
	my ($cb, $ecb, $url, $name) = @_;

	# keep track of the last firmware we requested, to prefetch it in the future
	my $releaseInfo = getFirmwareTag($url);

	$name ||= basename($url);

	if ($name !~ $FW_FILENAME_REGEX) {
		return $ecb->(undef, 'Unexpected firmware image name: ' . $name, $url, 400);
	}

	my $updatesDir = _getTempDir();
	my $firmwareFile = catfile($updatesDir, $name);

	if (-f $firmwareFile) {
		main::INFOLOG && $log->is_info && $log->info("Found uploaded firmware file $name");
		return $cb->($firmwareFile);
	}

	$updatesDir = Slim::Utils::OSDetect::dirsFor('updates');
	$firmwareFile = catfile($updatesDir, $name);

	if ($releaseInfo) {
		my $fileMatchRegex = join('-', '', $releaseInfo->{branch}, $releaseInfo->{model}, $releaseInfo->{res});
		Slim::Utils::Misc::deleteFiles($updatesDir, $fileMatchRegex, $firmwareFile);
	}

	if (-f $firmwareFile) {
		main::INFOLOG && $log->is_info && $log->info("Found cached firmware file");
		return $cb->($firmwareFile);
	}

	Slim::Networking::SimpleAsyncHTTP->new(
		sub {
			my $http = shift;

			if ($http->code != 200 || !-e "$firmwareFile.tmp") {
				return $ecb->($http, $http->mess);
			}

			rename "$firmwareFile.tmp", $firmwareFile or return $ecb->($http, "Unable to rename temporary $firmwareFile file" );

			return $cb->($firmwareFile);
		},
		sub {
			my ($http, $error) = @_;
			$http->code(404) if $error =~ /\b404\b/;
			$ecb->(@_);
		},
		{
			saveAs => "$firmwareFile.tmp",
		}
	)->get($url);

	return;
}

sub getFirmwareTag {
	my ($info) = @_;

	if (my ($model, $resolution, $version, $branch) = $info =~ $FW_TAG_REGEX) {
		my $releaseInfo = {
			model => $model,
			res => $resolution,
			version => $version,
			branch => $branch
		};

		return $releaseInfo;
	}
}

sub _errorDownloading {
	my ($httpClient, $response, $http, $error, $url, $code) = @_;

	$error ||= ($http && $http->error) || 'unknown error';
	$url   ||= ($http && $http->url) || 'no URL';
	$code  ||= ($http && $http->code) || 500;

	$log->error(sprintf("Failed to get data from Github: %s (%s)", $error || $http->error, $url));

	$response->headers->remove_content_headers;
	$response->code($code);
	$response->content_type('text/plain');
	$response->header('Connection' => 'close');
	$response->content('');

	$httpClient->send_response($response);
	Slim::Web::HTTP::closeHTTPSocket($httpClient);
};

sub handleFirmwareUpload {
	my ($httpClient, $response) = @_;

	my $request = $response->request;
	my $result = {};

	my $t = Time::HiRes::time();

	main::INFOLOG && $log->is_info && $log->info("New firmware image to upload. Size: " . formatMB($request->content_length));

	if ( $request->method !~ /HEAD|OPTIONS|POST/ ) {
		$log->error("Invalid HTTP verb: " . $request->method);
		$result = {
			error => 'Invalid request.',
			code  => 400,
		};
	}
	elsif ( $request->content_length > MAX_FW_IMAGE_SIZE ) {
		$log->error("Upload data is too large: " . $request->content_length);
		$result = {
			error => string('PLUGIN_DNDPLAY_FILE_TOO_LARGE', formatMB($request->content_length), formatMB(MAX_FW_IMAGE_SIZE)),
			code  => 413,
		};
	}
	else {
		my $ct = $request->header('Content-Type');
		my ($boundary) = $ct =~ /boundary=(.*)/;

		my ($uploadedFwFh, $filename, $inUpload, $buf);

		# open a pseudo-filehandle to the uploaded data ref for further processing
		open TEMP, '<', $request->content_ref;

		while (<TEMP>) {
			if ( Time::HiRes::time - $t > 0.2 ) {
				main::idleStreams();
				$t = Time::HiRes::time();
			}

			# a new part starts - reset some variables
			if ( /--\Q$boundary\E/i ) {
				$filename = '';

				if ($buf) {
					$buf =~ s/\r\n$//;
					print $uploadedFwFh $buf if $uploadedFwFh;
				}

				close $uploadedFwFh if $uploadedFwFh;
				$inUpload = undef;
			}

			# write data to file handle
			elsif ( $inUpload && $uploadedFwFh ) {
				print $uploadedFwFh $buf if defined $buf;
				$buf = $_;
			}

			# we got an uploaded file name
			elsif ( /filename="(.+?)"/i ) {
				$filename = $1;
				main::INFOLOG && $log->is_info && $log->info("New file to upload: $filename")
			}

			# we got the separator after the upload file name: file data comes next. Open a file handle to write the data to.
			elsif ( $filename && /^\s*$/ ) {
				$inUpload = 1;

				$uploadedFwFh = File::Temp->new(
					DIR => _getTempDir(),
					SUFFIX => '.bin',
					TEMPLATE => 'squeezelite-esp32-upload-XXXXXX',
					UNLINK => 0,
				) or $log->warn("Failed to open file: $@");

				binmode $uploadedFwFh;

				# remove file after a few minutes
				Slim::Utils::Timers::setTimer($uploadedFwFh->filename, Time::HiRes::time() + 15 * 60, sub { unlink shift });
			}
		}

		close TEMP;
		close $uploadedFwFh if $uploadedFwFh;

		main::idleStreams();

		if (!$result->{error}) {
			$result->{url} = _urlFromPath($uploadedFwFh->filename);
			$result->{size} = -s $uploadedFwFh->filename;
		}
	}

	$log->error($result->{error}) if $result->{error};

	my $content = to_json($result);
	$response->header( 'Content-Length' => length($content) );
	$response->code($result->{code} || 200);
	$response->header('Connection' => 'close');
	$response->content_type('application/json');

	Slim::Web::HTTP::addHTTPResponse( $httpClient, $response, \$content );
}

my $tempDir;
sub _getTempDir {
	return $tempDir if $tempDir;

	eval { $tempDir = Slim::Utils::Misc::getTempDir() };		# LMS 8.2+ only
	$tempDir ||= File::Temp::tempdir(CLEANUP => 1, DIR => preferences('server')->get('cachedir'));

	return $tempDir;
}

sub formatMB {
	return Slim::Utils::Misc::delimitThousands(int($_[0] / 1024 / 1024)) . 'MB';
}


1;