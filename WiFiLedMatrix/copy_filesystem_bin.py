import os
import subprocess
import shutil
import SCons.Script

# Pull the environment from the global SCons context
env = SCons.Script.DefaultEnvironment()

def copy_filesystem_bin(source, target, env):
    
    # Path where PlatformIO builds the binary
    build_dir = env.subst("$BUILD_DIR")
    fs_bin_path = os.path.join(build_dir, "littlefs.bin")
    
    # Path where you want to copy it (Project Root Folder)
    project_dir = env.subst("$PROJECT_DIR")
    destination_path = os.path.join(project_dir, "latest_filesystem.bin")
    
    if os.path.exists(fs_bin_path):
        shutil.copy2(fs_bin_path, destination_path)
        print(f"\n=======================================================")
        print(f"SUCCESS: Copied filesystem binary to:\n{destination_path}")
        print(f"=======================================================\n")
    else:
        print(f"\n[ERROR] could not find source file: {fs_bin_path}\n")

# Hook into the post-action of building the file system image
env.AddPostAction("$BUILD_DIR/littlefs.bin", copy_filesystem_bin)
