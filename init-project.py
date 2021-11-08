from subprocess import Popen, PIPE, STDOUT
import os
from pathlib import Path
import sys
import shutil

def get_script_dir():
    return Path(sys.path[0])


def is_windows():
    return os.name == "nt"


def runcmd(cmd, exit_on_failure=True, hide_command=False, hide_output=False):
    """
    Runs cmd
    :param cmd: Command to run
    :return: stdout as a list or None if an error occurred
    """
    if exit_on_failure:
        if "|" in cmd:
            cmd = cmd + "; test ${PIPESTATUS[0]} -eq 0"
        else:
            cmd = cmd + " || exit 1"

    if not hide_command:
        print(" > " + cmd)

    try:
        lines = []

        with Popen(cmd, stdout=PIPE, stderr=STDOUT, bufsize=1, universal_newlines=True, shell=True) as p:
            for line in p.stdout:
                if not hide_output:
                    print(line, end='')
                lines.append(line)

        if hide_command:
            cmd = ""

        if not p.returncode == 0 and exit_on_failure:
            print(cmd + " - Returned: " + str(p.returncode))
            exit(1)

        return [p.returncode, lines]
    except:
        if hide_command:
            cmd = ""

        print("Error executing cmd: " + cmd)

        if exit_on_failure:
            exit(1)

        return None


def check_application_exists(name):
    """Check whether `name` is on PATH and marked as executable."""
    from shutil import which

    return which(name) is not None


def get_projucer(juce_path):
    if not juce_path.exists():
        raise RuntimeError("Couldn't find JUCE path - Make sure to init submodules, run - git submodule update --init --recursive")

    os_type = "VisualStudio2019" if is_windows() else "MacOSX"
    extension = "exe" if is_windows() else "app"

    projucer_root = juce_path / f"extras/Projucer/Builds/{os_type}"
    projucer_app = projucer_root / f"build/Debug/Projucer.{extension}"

    if not projucer_app.exists():
        print("-- Building Projucer - This will take a few minutes")
        if not is_windows():
            build_cmd = f"xcodebuild -project {projucer_root / 'Projucer.xcodeproj'} -configuration Debug"
            runcmd(build_cmd, exit_on_failure=True, hide_output=True, hide_command=False)

    if not is_windows():
        projucer_app = projucer_app / "Contents/MacOS/Projucer"

    return projucer_app


def resave_project(projucer, project):
    print("-- Saving Project Files")
    cmd = f'"{projucer}" --resave "{project}"'
    runcmd(cmd, hide_output=True)


def get_project_name(projucer, project):
    err, lines = runcmd(f'"{projucer}" --status "{project}"', hide_output=True, hide_command=True)
    search_str = "Name: "
    for line in lines:
        if search_str in line:
            return (line[len(search_str):-1])


def ios_setup(projucer, project):
    if is_windows():
        return

    if not check_application_exists("pod"):
        raise RuntimeError("Couldn't find cocoa pods - Please install it")

    ios_dir = get_script_dir() / "ios"

    cwd = os.getcwd()
    os.chdir(ios_dir)

    print("-- Installing Pods")
    pod_cmd = "pod install"
    runcmd(pod_cmd)

    os.chdir(cwd)

    # Patch xcodeproj
    project_name = get_project_name(projucer, project)
    pbx_file = ios_dir / f"{project_name}.xcodeproj/project.pbxproj"

    with open(pbx_file, 'r') as file:
        lines = file.readlines()

    output = []

    adding_build_phases = False
    build_phase_number = 0

    remove_pods_lib = False

    for line in lines:
        if "/* Begin PBXFrameworksBuildPhase section */" in line:
            remove_pods_lib = True

        if f"libPods-{project_name}" in line and remove_pods_lib:
            remove_pods_lib = False
            output.append("\t\t\t\tA124F7FF2736C766007F6F35 /* OpenSSL.xcframework in Frameworks */,\n")
            output.append("\t\t\t\tA124F7FC2736C761007F6F35 /* double-conversion.xcframework in Frameworks */,\n")
            continue

        if 'INFOPLIST_FILE = "Info-App.plist";' in line:
            output.append("LD_RUNPATH_SEARCH_PATHS = /usr/lib/swift;\n")

        if "/* Main.cpp */" in line:
            remove_str = "lastKnownFileType = sourcecode.cpp.cpp;"
            if remove_str in line:
                line = line.replace(remove_str, "lastKnownFileType = sourcecode.cpp.objcpp;")

        if "buildPhases = (" in line:
            adding_build_phases = True

        if adding_build_phases:
            build_phase_number += 1

            if build_phase_number == 3:
                output.append("\t\t\t\tA124F6112736B3C4007F6F35 /* Start Packager */,\n")
            elif build_phase_number == 5:
                output.append("\t\t\t\tA124F6122736B3DA007F6F35 /* Bundle React Native code and images */,\n")
            elif build_phase_number > 5:
                adding_build_phases = False

        if "/* Begin PBXShellScriptBuildPhase section */" in line:
            output.append(line)
            # Ensure any shell scripts are fully escaped
            output.append("""		A124F6112736B3C4007F6F35 /* Start Packager */ = {
			isa = PBXShellScriptBuildPhase;
			buildActionMask = 2147483647;
			files = (
			);
			inputFileListPaths = (
			);
			inputPaths = (
			);
			name = "Start Packager";
			outputFileListPaths = (
			);
			outputPaths = (
			);
			runOnlyForDeploymentPostprocessing = 0;
			shellPath = /bin/sh;
			shellScript = "export RCT_METRO_PORT=\\"${RCT_METRO_PORT:=8081}\\"\\necho \\"export RCT_METRO_PORT=${RCT_METRO_PORT}\\" > \\"${SRCROOT}/../node_modules/react-native/scripts/.packager.env\\"\\nif [ -z \\"${RCT_NO_LAUNCH_PACKAGER+xxx}\\" ] ; then\\n  if nc -w 5 -z localhost ${RCT_METRO_PORT} ; then\\n    if ! curl -s \\"http://localhost:${RCT_METRO_PORT}/status\\" | grep -q \\"packager-status:running\\" ; then\\n      echo \\"Port ${RCT_METRO_PORT} already in use, packager is either not running or not running correctly\\"\\n      exit 2\\n    fi\\n  else\\n    open \\"$SRCROOT/../node_modules/react-native/scripts/launchPackager.command\\" || echo \\"Can't start packager automatically\\"\\n  fi\\nfi\\n";
			showEnvVarsInLog = 0;
		};\n""")
            output.append("""		A124F6122736B3DA007F6F35 /* Bundle React Native code and images */ = {
			isa = PBXShellScriptBuildPhase;
			buildActionMask = 2147483647;
			files = (
			);
			inputFileListPaths = (
			);
			inputPaths = (
			);
			name = "Bundle React Native code and images";
			outputFileListPaths = (
			);
			outputPaths = (
			);
			runOnlyForDeploymentPostprocessing = 0;
			shellPath = /bin/sh;
			shellScript = "set -e\\n\\nexport NODE_BINARY=node\\n../node_modules/react-native/scripts/react-native-xcode.sh\\n";
			showEnvVarsInLog = 0;
		};\n""")
            continue


        output.append(line)

    with open(pbx_file, 'w') as file:
        file.writelines(output)


def android_setup(projucer, project):
    juce_files_root = get_script_dir() / "Builds" / "Android"
    rn_files_root = get_script_dir() / "android"

    shutil.copy2(juce_files_root / "local.properties", rn_files_root / "local.properties")

    (rn_files_root / "cpp").mkdir(parents=True, exist_ok=True)
    shutil.copy2(juce_files_root / "app" / "CMakeLists.txt", rn_files_root / "app" / "cpp" / "CMakeLists.txt")

    # Find JUCE java files
    with open(juce_files_root / "app" / "build.gradle", "r") as f:
        lines = f.readlines()

        found_juce_files = False
        juce_jave_files = []

        for line in lines:
            if "main.java.srcDirs +=" in line:
                found_juce_files = True
                continue

            if found_juce_files:
                if len(line.strip()) == 0:
                    break
                juce_jave_files.append(line.replace("[", "").replace("\"", "").replace("]", "").replace(",", "").strip())

    absolute_juce_jave_files = [(juce_files_root / "app" / f).resolve() for f in juce_jave_files]
    copy_files = []

    for dir in absolute_juce_jave_files:
        files = dir.rglob("*.java")
        for f in files:
            if "JuceApp.java" in str(f) or "JuceActivity.java" in str(f):
                continue
            copy_files.append(f)

    for file in copy_files:
        output = get_script_dir() / "android" / "app" / "src" / "main" / "java"

        new_path_part = str(file)[str(file).index("com/rmsl"):]
        new_path = output / new_path_part

        new_path.parent.mkdir(exist_ok=True, parents=True)
        shutil.copy2(file, new_path)


juce_path = get_script_dir() / "JUCE"
projucer = get_projucer(juce_path)

resave_project(projucer, get_script_dir() / "RNJuce.jucer")

ios_setup(projucer, get_script_dir() / "RNJuce.jucer")
android_setup(projucer, get_script_dir() / "RNJuce.jucer")
