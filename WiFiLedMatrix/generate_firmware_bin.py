import os
import subprocess
import shutil
import SCons.Script

# Pull the environment from the global SCons context
env = SCons.Script.DefaultEnvironment()

def bin_formatter(source, target, env):
    # Safely unwrap the SCons target list and get the real absolute path string
    elf_node = target[0]
    elf_file = os.path.abspath(elf_node.get_path())
    
    # Define paths for the internal build directory and the destination folder
    build_dir = os.path.dirname(elf_file)
    bin_file = os.path.join(build_dir, "firmware.bin")
    
    # Destination is the project root directory
    project_dir = env.get("PROJECT_DIR")
    handy_bin_file = os.path.join(project_dir, "latest_firmware.bin")
    
    # Locate the toolchain's objcopy executable
    objcopy = env.subst("$OBJCOPY")
    
    print("\n--- CUSTOM SCRIPT: Generating raw binary for ElegantOTA ---")
    print(f"Source ELF: {elf_file}")
    
    # Run the toolchain objcopy command to extract raw binary data
    result = subprocess.run([objcopy, "-O", "binary", elf_file, bin_file])
    
    if result.returncode == 0:
        # Copy the file to the handy project root directory
        shutil.copyfile(bin_file, handy_bin_file)
        print("--- SUCCESS: Firmware exported to handy location! ---")
        print(f"Location: {handy_bin_file}")
        print(f"Size: {os.path.getsize(handy_bin_file)} bytes\n")
    else:
        print("--- ERROR: Failed to generate firmware.bin ---\n")


# Hook into the Program target cleanly
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", bin_formatter)
