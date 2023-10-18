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

fetch_cmd = 'fetch'
gclient_cmd = 'gclient'
gn_cmd = 'gn'

target_platform = sys.platform

if sys.platform.startswith('linux'):
  target_platform = 'linux'
elif sys.platform.startswith('win32'):
  target_platform = 'win32'
  fetch_cmd = 'fetch.bat'
  gclient_cmd = 'glient.bat'
  gn_cmd = 'gn.bat'

target_os = target_platform
target_cpu = platform.machine()

if (target_cpu == 'x86_64'):
  target_cpu = 'x64'
elif (target_cpu == 'i386'):
  target_cpu = 'x86'

release_name = 'master'

try:
  release_name = ''.join(subprocess.check_output(['git', 'describe', '--abbrev=0', '--tags']).split())
except ValueError:
  release_name = 'master'

if (os.environ.get('WEBRTC_TARGET_OS')):
  target_os = os.environ['WEBRTC_TARGET_OS']

if (os.environ.get('WEBRTC_TARGET_CPU')):
  target_cpu = os.environ['WEBRTC_TARGET_CPU']

pkg_name = 'libcrtc-' + release_name + '-' + target_os  + '-' + target_cpu + '.tar.gz'

def build_archive():
  with tarfile.open(os.path.join(dist_dir, pkg_name), "w:gz") as tar:
    tar.add(dist_include_dir, arcname='include')
    tar.add(dist_lib_dir, arcname='lib')
    tar.add(os.path.join(dist_dir, 'LICENSE'), arcname='LICENSE')
    tar.add(os.path.join(dist_dir, 'WEBRTC_LICENSE'), arcname='WEBRTC_LICENSE')
    tar.add(os.path.join(dist_dir, 'WEBRTC_LICENSE_THIRD_PARTY'), arcname='WEBRTC_LICENSE_THIRD_PARTY')

depot_tools_dir = os.path.join(third_party_dir, 'depot_tools')
webrtc_dir = os.path.join(third_party_dir, 'webrtc')
webrtc_src_dir = os.path.join(webrtc_dir, 'src')
webrtc_crtc_dir = os.path.join(webrtc_src_dir, 'crtc')
webrtc_sync = os.path.join(third_party_dir, '.webrtc_sync')

if not os.path.exists(third_party_dir):
  os.mkdir(third_party_dir)

if not os.path.exists(depot_tools_dir):
  subprocess.check_call(['git', 'clone', 'https://chromium.googlesource.com/chromium/tools/depot_tools.git', depot_tools_dir])

os.environ['PATH'] += os.pathsep + depot_tools_dir

if not os.path.exists(webrtc_dir):
  os.mkdir(webrtc_dir)

if not os.path.exists(webrtc_sync):
  os.chdir(webrtc_dir)

  if not os.path.exists(webrtc_src_dir):
    subprocess.call([fetch_cmd, '--nohooks', 'webrtc'])
  else:
    os.chdir(webrtc_src_dir)

    subprocess.check_call(['git', 'fetch', 'origin'])
    subprocess.check_call(['git', 'reset', '--hard', 'origin/master'])
    subprocess.check_call(['git', 'checkout', 'origin/master'])
    
    if subprocess.check_output(['git', 'branch', '--list', 'libcrtc']):
      subprocess.check_call(['git', 'branch', '-D', 'libcrtc'])

    subprocess.check_call(['git', 'clean', '-f'])

    os.chdir(webrtc_dir)
  
  subprocess.check_call([gclient_cmd, 'sync', '--with_branch_heads', '--force'])

  os.chdir(webrtc_src_dir)

  subprocess.check_call(['git', 'checkout', '-b', 'libcrtc', 'refs/remotes/branch-heads/59'])

  if os.path.exists(os.path.join(webrtc_src_dir, 'BUILD.gn')):
    os.remove(os.path.join(webrtc_src_dir, 'BUILD.gn'))

  if not os.path.exists(webrtc_crtc_dir):
    os.symlink(root_dir, webrtc_crtc_dir)

  os.symlink(os.path.join(root_dir, 'root.gn'), os.path.join(webrtc_src_dir, 'BUILD.gn'))
  open(webrtc_sync, 'a').close()

  os.chdir(root_dir)

os.chdir(webrtc_src_dir)
gn_flags = '--args=rtc_include_tests=false is_component_build=false rtc_use_h264=true rtc_enable_protobuf=false treat_warnings_as_errors=false use_custom_libcxx=false'

if os.environ.get('WEBRTC_DEBUG') == 'true':
  gn_flags += ' is_debug=true'
else:
  gn_flags += ' is_debug=false'

if (target_os == 'linux'):
  gn_flags += ' use_ozone=true'
  gn_flags += ' is_desktop_linux=false'
  gn_flags += ' rtc_use_gtk=false'

if (target_os != target_platform):
  gn_flags += ' target_os="' + target_os + '"'

if (target_cpu != platform.machine()):
  gn_flags += ' target_cpu="' + target_cpu + '"'

subprocess.check_call([gn_cmd, 'gen', os.path.join(out_dir, target_os, target_cpu), gn_flags])
os.chdir(root_dir)

if os.environ.get('WEBRTC_EXAMPLES') == 'true':
  subprocess.check_call(['ninja', '-C', os.path.join(out_dir, target_os, target_cpu), 'crtc-examples'])
else:
  subprocess.check_call(['ninja', '-C', os.path.join(out_dir, target_os, target_cpu), 'crtc'])

if not os.path.exists(dist_dir):
  os.mkdir(dist_dir)

if os.path.exists(dist_include_dir):
  shutil.rmtree(dist_include_dir)

os.mkdir(dist_include_dir)

if os.path.exists(dist_lib_dir):
  shutil.rmtree(dist_lib_dir)
  
os.mkdir(dist_lib_dir)

shutil.copy(os.path.join(root_dir, 'include', 'crtc.h'), dist_include_dir)
shutil.copyfile(os.path.join(root_dir, 'LICENSE'), os.path.join(dist_dir, 'LICENSE'))
shutil.copyfile(os.path.join(webrtc_src_dir, 'webrtc', 'LICENSE'), os.path.join(dist_dir, 'WEBRTC_LICENSE'))
shutil.copyfile(os.path.join(webrtc_src_dir, 'webrtc', 'LICENSE_THIRD_PARTY'), os.path.join(dist_dir, 'WEBRTC_LICENSE_THIRD_PARTY'))

if sys.platform.startswith('linux'):
  shutil.copy(os.path.join(os.path.join(out_dir, target_os, target_cpu), 'libcrtc.so'), dist_lib_dir)

elif sys.platform.startswith('win32'):
  shutil.copy(os.path.join(os.path.join(out_dir, target_os, target_cpu), 'libcrtc.dll'), dist_lib_dir)

elif sys.platform.startswith('darwin'):
  shutil.copy(os.path.join(os.path.join(out_dir, target_os, target_cpu), 'libcrtc.dylib'), dist_lib_dir)

build_archive()