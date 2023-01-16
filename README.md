# Elastic CGRA Mapper
mapping tool for Elastic CGRA


## requirement (currently ikura env)
GCC 8.5.0 x86_64-redha-linux

cmake 3.20.2

## build
### 1. setup
CMakeLists.txtの以下の部分を設定
```cmake
set(ENV{GUROBI_HOME} /opt/gurobi911/linux64)
```