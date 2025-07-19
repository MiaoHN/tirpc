FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list
RUN sed -i 's/security.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

RUN apt-get update
RUN apt-get install -y build-essential cmake gdb valgrind git vim
RUN apt-get install -y libtinyxml2-dev libzookeeper-mt-dev libyaml-cpp-dev libprotobuf-dev protobuf-compiler libboost-all-dev

WORKDIR /workspace