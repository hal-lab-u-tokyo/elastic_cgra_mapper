# Elastic CGRA Mapper
mapping tool for Elastic CGRA


## requirement (currently confirmed to work)
GCC >= 8.5.0 

gurobi = 9.1.1

cmake >= 3.20.2

## build
### 1. setup
CMakeLists.txtの以下の部分を設定
```cmake
set(ENV{GUROBI_HOME} /opt/gurobi911/linux64)
```

## mapping
```bash
cd build
./main
```

## visualizer 
```bash
cd visualizer
dot -Tpng {input/dotfile} -o {output/pngfile} # visualize dot file
python3 main.py {input/mapping/json} # visualize mapping result
```
