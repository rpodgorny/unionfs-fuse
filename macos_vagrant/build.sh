#!/bin/sh
set -e -x
vagrant destroy --force || true
rm -rf Vagrantfile

VERSION=10.12.6

# unfortunately this was the only macos-based image that works on my linux system :-(
vagrant init --box-version ${VERSION} --minimal jhcook/macos-sierra
vagrant up

vagrant upload ./install-xcode-cli-tools.sh ./

echo '
set -e -x

./install-xcode-cli-tools.sh
rm ./install-xcode-cli-tools.sh

curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh >install.sh
chmod a+x install.sh
./install.sh
rm ./install.sh' | vagrant ssh

# TODO: find out why this is useless (not being sourced on vagrant ssh login)
echo 'echo "export PATH=/usr/local/bin:$PATH" >>~/.bash_profile && source ~/.bash_profile' | vagrant ssh
echo 'echo "export PATH=/usr/local/bin:$PATH" >>~/.bashrc && source ~/.bashrc' | vagrant ssh
#echo 'echo "export PATH=/usr/local/bin:$PATH" >>~/.profile && source ~/.profile' | vagrant ssh

# TODO: don't source
echo 'source .bashrc; brew install macfuse cmake python3' | vagrant ssh

vagrant halt
rm -rf the.box
vagrant package --output the.box --vagrantfile custom_vagrantfile
#vagrant package --output the.box
ls -l the.box
vagrant cloud publish --force --release rpodgorny/mymacos ${VERSION}.0 virtualbox the.box
rm -rf the.box

vagrant destroy --force
rm -rf Vagrantfile
