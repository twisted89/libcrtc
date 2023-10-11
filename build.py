#!/usr/bin/env python

import os
import sys
import subprocess
import shutil
import tarfile
import platform



root_dir = os.path.dirname(os.path.realpath(__file__))
dist_dir = os.path.join(root_dir, 'dist')
dist_include_dir = os.path.join(dist_dir, 'include')
dist_lib_dir = os.path.join(dist_dir, 'lib')
out_dir = os.path.join(root_dir, 'out')
third_party_dir = os.path.join(root_dir, '3dparty')

target_platform = sys.platform

if sys.platform.startswith('linux'):
  target_platform = 'linux'

target_os = target_platform
target_cpu = platform.machine()

if (target_cpu == 'x86_64'):
  target_cpu = 'x64'
elif (target_cpu == 'i386'):
  target_cpu = 'x86'

release_name = ''.join(str(subprocess.check_output(['git', 'describe', '--abbrev=0', '--tags']).split()))

if (os.environ.get('WEBRTC_TARGET_OS')):
  target_os = os.environ['WEBRTC_TARGET_OS']

if (os.environ.get('WEBRTC_TARGET_CPU')):
  target_cpu = os.environ['WEBRTC_TARGET_CPU']

pkg_name = 'libcrtc-' + release_name + '-' + target_os  + '-' + target_cpu + '.tar.gz'

def build_archive():
  with tarfile.open(os.path.join(dist_dir, pkg_name), "w:gz") as tar:
    tar.add(dist_include_dir, arcname='include')
    tar.add(dist_lib_dir, arcname='lib')

depot_tools_dir = os.path.join(third_party_dir, 'depot_tools')
webrtc_dir = os.path.join(third_party_dir, 'webrtc')
webrtc_src_dir = os.path.join(webrtc_dir, 'src')
webrtc_crtc_dir = os.path.join(webrtc_src_dir, 'crtc')
webrtc_sync = os.path.join(third_party_dir, '.webrtc_sync')

gn_flags = '--args=is_debug=false rtc_include_tests=false use_ozone=true is_desktop_linux=false rtc_use_gtk=false clang_use_chrome_plugins=true rtc_enable_protobuf=false'

if (target_os != target_platform):
  gn_flags += ' target_os="' + target_os + '"'

if (target_cpu != platform.machine()):
  gn_flags += ' target_cpu="' + target_cpu + '"'

if not os.path.exists(third_party_dir):
  os.mkdir(third_party_dir)

if not os.path.exists(depot_tools_dir):
  subprocess.call(['git', 'clone', 'https://chromium.googlesource.com/chromium/tools/depot_tools.git', depot_tools_dir])

os.environ['PATH'] += os.pathsep + depot_tools_dir

if sys.platform.startswith('win32'):
  os.environ['vs2022_install'] = 'C:\Program Files\Microsoft Visual Studio\2022\Professional'
  os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

if not os.path.exists(webrtc_dir):
  os.mkdir(webrtc_dir)

if not os.path.exists(webrtc_sync):
  os.chdir(webrtc_dir)

  if not os.path.exists(webrtc_src_dir):
    if sys.platform.startswith('win32'):
        subprocess.call(['fetch.bat', '--nohooks', '--no-history', 'webrtc'])
    else:
        subprocess.call(['fetch', '--nohooks', '--no-history', 'webrtc'])
  else:
    os.chdir(webrtc_src_dir)

    subprocess.call(['git', 'fetch', 'origin'])
    subprocess.call(['git', 'reset', '--hard', 'origin/master'])
    subprocess.call(['git', 'checkout', 'origin/master'])
    
    if subprocess.check_output(['git', 'branch', '--list', 'libcrtc']):
      subprocess.call(['git', 'branch', '-D', 'libcrtc'])

    subprocess.call(['git', 'clean', '-f'])

    os.chdir(webrtc_dir)
  
  if sys.platform.startswith('win32'):
   subprocess.call(['gclient.bat', 'sync', '--with_branch_heads', '--force'])
  else:
    subprocess.call(['gclient', 'sync', '--with_branch_heads', '--force'])

  os.chdir(webrtc_src_dir)

  #subprocess.call(['git', 'checkout', '-b', 'libcrtc', 'refs/remotes/branch-heads/58'])
  #subprocess.call(['git', 'checkout', '-b', 'libcrtc', 'main'])

  if os.path.exists(os.path.join(webrtc_src_dir, 'BUILD.gn')):
    os.remove(os.path.join(webrtc_src_dir, 'BUILD.gn'))

  if not os.path.exists(webrtc_crtc_dir):
    os.symlink(root_dir, webrtc_crtc_dir)

  os.symlink(os.path.join(root_dir, 'root.gn'), os.path.join(webrtc_src_dir, 'BUILD.gn'))
  open(webrtc_sync, 'a').close()

  os.chdir(root_dir)

os.chdir(webrtc_src_dir)
if sys.platform.startswith('win32'):
  subprocess.call(['gn.bat', 'gen', out_dir, gn_flags])
else:
  subprocess.call(['gn', 'gen', out_dir, gn_flags])
os.chdir(root_dir)

if os.environ.get('BUILD_WEBRTC_EXAMPLES') == 'true':
  if sys.platform.startswith('win32'):
    subprocess.call(['ninja.bat', '-C', 'out', 'crtc-examples'])
  else:
    subprocess.call(['ninja', '-C', 'out', 'crtc-examples'])
else:
  if sys.platform.startswith('win32'):
    subprocess.call(['ninja.bat', '-C', 'out', 'crtc'])
  else:
    subprocess.call(['ninja', '-C', 'out', 'crtc'])

if os.path.exists(dist_dir):
  shutil.rmtree(dist_dir)

os.mkdir(dist_dir)
os.mkdir(dist_include_dir)
os.mkdir(dist_lib_dir)

shutil.copy(os.path.join(root_dir, 'include', 'crtc.h'), dist_include_dir)

if sys.platform.startswith('linux'):
  shutil.copy(os.path.join(out_dir, 'libcrtc.so'), dist_lib_dir)
  shutil.copy(os.path.join(out_dir, 'libboringssl.so'), dist_lib_dir)

elif sys.platform.startswith('win32'):
  shutil.copy(os.path.join(out_dir, 'libcrtc.dll'), dist_lib_dir)
  shutil.copy(os.path.join(out_dir, 'libboringssl.dll'), dist_lib_dir)

elif sys.platform.startswith('darwin'):
  shutil.copy(os.path.join(out_dir, 'libcrtc.dylib'), dist_lib_dir)
  shutil.copy(os.path.join(out_dir, 'libboringssl.dylib'), dist_lib_dir)

build_archive()
