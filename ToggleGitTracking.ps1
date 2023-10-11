param (
    [Parameter(Position=0, Mandatory=$false)]
    [ValidateSet("t", "u")]
    [string]$option
)

# Define the directory to apply changes to
$targetDir = "components\wifi-manager\webapp\dist"

# Get the current directory
$currentDir = Get-Location

# Get list of files from the file system
$fsFiles = Get-ChildItem -Recurse $targetDir -File | ForEach-Object {
    $_.FullName.Substring($currentDir.Path.Length + 1).Replace("\", "/")
}

# Get list of files from the Git index
$indexFiles = git ls-files -s $targetDir | ForEach-Object {
    ($_ -split "\s+")[3]
}

# Combine and remove duplicates
$allFiles = $fsFiles + $indexFiles | Sort-Object -Unique

# Apply the git command based on the option
$allFiles | ForEach-Object {
    $relativePath = $_
    $isInIndex = $indexFiles -contains $relativePath

    if ($null -eq $option) {
        $status = if ($isInIndex) { 'tracked' } else { 'not tracked' }
        Write-Host "$relativePath is $status"
    }
    elseif ($isInIndex) {
        if ($option -eq "t") {
            git update-index --no-skip-worktree $relativePath
            Write-Host "Started tracking changes in $relativePath"
        }
        elseif ($option -eq "u") {
            git update-index --skip-worktree $relativePath
            Write-Host "Stopped tracking changes in $relativePath"
        }
    }
    else {
        Write-Host "File $relativePath is not tracked."
    }
}
