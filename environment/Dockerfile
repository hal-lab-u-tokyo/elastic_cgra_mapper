FROM gurobi/optimizer:9.1.1

RUN apt-get update && \
    apt-get install -y wget

ARG USER_NAME=ubuntu
ARG PROJECT_NAME=elastic_cgra_mapper
ARG APPLICATION_DIRECTORY=/home/${USER_NAME}/${PROJECT_NAME}

RUN wget -qO- "https://cmake.org/files/v3.20/cmake-3.20.2-linux-x86_64.tar.gz" | tar --strip-components=1 -xz -C /usr/local

RUN apt-get update && \
    apt-get install -y \ 
    build-essential \
    clang \
    clang-format \
    vim \
    git \
    ninja-build \
    gdb \ 
    python3-pip 
RUN apt-get install -y verilator

ENV PATH="/home/verilator/bin:${PATH}"
ENV GRB_LICENSE_FILE="/opt/gurobi/gurobi.lic"

COPY ../requirements.txt ${APPLICATION_DIRECTORY}/requirements.txt
RUN pip install -r ${APPLICATION_DIRECTORY}/requirements.txt

RUN useradd -ms /bin/sh ${USER_NAME}
RUN chown -R ${USER_NAME} /home/${USER_NAME}
USER ${USER_NAME}

WORKDIR ${APPLICATION_DIRECTORY}