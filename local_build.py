import shutil
from i
out_folder = "../TAudio-case/builds/"
name = "tembed"
platform = "esp32s3"
depth = "32"
version =  f"{name}.{platform}.{depth}-v2023.10.15"
merged_bin = f"squeezelite-{name}.{platform}.{depth}.bin"
print(f"Copying {merged_bin} to project folder")
shutil.copyfile("build/" + merged_bin, out_folder+merged_bin)
