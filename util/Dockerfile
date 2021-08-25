FROM ubuntu:18.04

RUN set -e

RUN export GIT_SSL_NO_VERIFY=1
RUN apt update
RUN apt -y install build-essential linux-headers-generic wget tar libboost-dev cmake

RUN apt -y install git
RUN apt -y install software-properties-common
RUN add-apt-repository ppa:git-core/ppa
RUN apt update
RUN apt -y install git

ENV PROMPT_COMMAND='history -a'
