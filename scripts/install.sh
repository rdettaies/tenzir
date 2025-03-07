#!/bin/sh
#
# This script installs the latest release of Tenzir.
#

# Treat unset variables as an error when substituting.
set -u

# Abort on error.
set -e

#
# Helper utilities
#

# Check whether a command exists.
check() {
  command -v "$1" >/dev/null 2>&1
}

# Only use colors when we have a TTY.
if [ -t 1 ]
then
  escape() { printf "\033[%sm" "$1"; }
else
  escape() { :; }
fi
make_bold() { escape "1;$1"; }
green="$(make_bold 32)"
blue="$(make_bold 34)"
bold="$(make_bold 39)"
normal="$(escape 0)"

# Prints an "action", i.e., something the script is doing.
# Use this to transparently inform the user about the key
# steps this script takes.
action() {
  printf "${blue}==>${bold} %s${normal}\n" "$@"
}

# Only prompt for user confirmation if we have a TTY.
confirm() {
  if [ -t 1 ]
  then
    echo
    echo "Press ${green}ENTER${normal} to continue."
    read -r response < /dev/tty
  fi
}

# Print welcome banner.
echo "${blue}"
echo "                _____ _____ _   _ ________ ____  "
echo "               |_   _| ____| \ | |__  /_ _|  _ \ "
echo "                 | | |  _| |  \| | / / | || |_) |"
echo "                 | | | |___| |\  |/ /_ | ||  _ < "
echo "                 |_| |_____|_| \_/____|___|_| \\_\\"
echo "${normal}"
echo "                            INSTALLER"
echo

# Detect OS.
action "Identifying platform"
platform=
os=$(uname -s)
if [ "${os}" = "Linux" ]
then
    if [ -f "/etc/debian_version" ]
    then
        platform=Debian
    elif [ -f "/etc/NIXOS" ]
    then
        platform=NixOS
    else
        platform=Linux
    fi
elif [ "${os}" = "Darwin" ]
then
  platform=macOS
elif [ -z "${os}" ]
then
  echo "Could not identify platform."
  exit 1
fi
echo "Found ${platform}."

# Select appropriate package.
action "Identifying package"
package=
package_url=
if [ "${platform}" = "Debian" ]
then
  package_url="https://storage.googleapis.com/tenzir-dist-public/packages/main/debian/tenzir-static-latest.deb"
elif [ "${platform}" = "Linux" ]
then
  package_url="https://storage.googleapis.com/tenzir-dist-public/packages/main/tarball/tenzir-static-latest.gz"
elif [ "${platform}" = "NixOS" ]
then
  echo "Try Tenzir with our ${bold}flake.nix${normal}:"
  echo
  echo "    ${bold}nix shell github:tenzir/tenzir/latest -c tenzir-node${normal}"
  echo
  echo "Install Tenzir by adding" \
    "${bold}github:tenzir/tenzir/latest${normal} to your"
  echo "flake inputs, or use your preferred method to include third-party"
  echo "modules on classic NixOS."
  exit 0
else
  echo "We do not offer pre-built packages for ${platform}." \
      "Your options:"
  echo
  echo "  1. Use Docker"
  echo "  2. Build from source"
  echo
  echo "Visit ${bold}https://docs.tenzir.com${normal} for further instructions."
  exit 1
fi
package="$(basename "${package_url}")"
echo "Using ${package}"

# Download package.
tmpdir="$(dirname "$(mktemp -u)")"
action "Downloading ${package_url}"
if check wget
then
  wget -q --show-progress -O "${tmpdir}/${package}" "${package_url}"
elif check curl
then
  curl --progress-bar -L -o "${tmpdir}/${package}" "${package_url}"
else
  echo "Neither ${bold}wget${normal} nor ${bold}curl${normal}" \
    "found in \$PATH."
  exit 1
fi
echo "Successfully downloaded ${package}"

# Get platform config.
open_source=
prefix=/opt/tenzir
config="${prefix}/etc/tenzir/plugin/platform.yaml"
echo
echo "Unlock the full potential of your Tenzir node as follows:"
echo
echo "  1. Go to ${bold}https://app.tenzir.com${normal}"
echo "  2. Sign up for a free account"
echo "  3. Configure a node and download a ${bold}platform.yaml${normal} config"
echo "  4. Move the config to ${bold}${config}${normal}"
echo
echo "(You can skip these steps, but then only open source features will be"
echo "available. Visit ${bold}https://tenzir.com/pricing${normal} for a" \
  "feature comparison.)"
confirm

# Check for platform configuration.
action "Checking for existence of platform configuration"
if ! [ -f "${config}" ]
then
  echo "Could not find config at ${bold}${config}${normal}."
  action "Using open source feature set"
  open_source=1
else
  echo "Found config at ${bold}${config}${normal}."
  action "Using complete feature set"
fi

# Trigger installation.
action "Installing package into ${prefix}"
if ! check sudo
then
  echo "Could not find ${bold}sudo${normal} in \$PATH."
  exit 1
fi
if [ "${platform}" = "Debian" ]
then
  # adduser is required by the Debian package installation.
  if ! check adduser
  then
    echo "Could not find ${bold}adduser${normal} in \$PATH."
    exit 1
  fi
  cmd1="sudo dpkg -i \"${tmpdir}/${package}\""
  cmd2="sudo systemctl status tenzir-node || [ ! -d /run/systemd/system ]"
  echo "This script is about to run the following commands:"
  echo
  echo "  - ${cmd1}"
  echo "  - ${cmd2}"
  confirm
  action "Installing via dpkg"
  eval "${cmd1}"
  action "Checking node status"
  eval "${cmd2}"
elif [ "${platform}" = "Linux" ]
then
  cmd1="sudo tar xzf \"${tmpdir}/${package}\" -C /"
  echo "This script is about to run the following command:"
  echo
  echo "  - ${cmd1}"
  confirm
  action "Unpacking tarball"
  eval "${cmd1}"
fi

# Test the installation.
action "Checking version"
PATH="${prefix}/bin:$PATH"
tenzir -q 'show version | write json'

# Inform about next steps.
action "Providing guidance"
echo "You're all set! Next steps:"
echo
echo "  - Ensure that ${bold}${prefix}/bin${normal} is in your \$PATH"
echo "  - Run a pipeline via ${green}tenzir <pipeline>${normal}"
if [ "${platform}" = "Linux" ]
then
  echo "  - Spawn a node via ${green}tenzir-node${normal}"
fi
if [ -z "${open_source}" ]
then
  echo "  - Explore your node and manage pipelines at" \
    "${bold}https://app.tenzir.com${normal}"
fi
echo "  - Follow the guides at ${bold}https://docs.tenzir.com${normal}"
echo "  - Swing by our friendly Discord at" \
  "${bold}https://discord.tenzir.com${normal}"
