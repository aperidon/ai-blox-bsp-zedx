#!/bin/bash
# if docker needs sudo on your ubuntu machine, you need to run these steps : 
# https://docs.docker.com/engine/install/linux-postinstall/
SUDO_CMD=
if [[ -z ${CI_COMMIT_REF_NAME} ]]; then
    SUDO_CMD=sudo
fi

if [ "$(uname -m)" == "x86_64" ]; then
echo "installing qemu"
$SUDO_CMD apt-get install qemu binfmt-support qemu-user-static
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
fi

echo "Building daemon for jetpack $1..."
## build the application in a docker container and copy the output result into the app folder
#DOCKER_BUILDKIT=1 && docker build -t mybuildimage --network host --file Daemon/Dockerfile_$1 --output ./Daemon ./
DOCKER_BUILDKIT=1 && docker build -t mybuildimage --network host --file Daemon/Dockerfile_$1 ./
container_id=$(docker create mybuildimage)
docker cp $container_id:/ZEDX_Daemon ./Daemon/
#docker rm $container_id
 


