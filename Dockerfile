FROM ubuntu:20.04 as base

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get install -y tzdata
RUN apt-get install -y build-essential pkg-config cmake libglfw3-dev libopenal-dev
