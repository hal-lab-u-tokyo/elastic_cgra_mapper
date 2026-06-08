# Elastic CGRA Mapper
mapping tool for Elastic CGRA

## requirement (currently confirmed to work)
GCC >= 8.5.0, gurobi = 9.1.1, cmake >= 3.20.2

## 1. Setup Environment
- clone this repository to your $HOME directory
- Download gurobi WLS license file from [Web License Manager](https://license.gurobi.com/manager/licenses) and place it to license_files directory
- build and run docker container using the following command
```docker
cd environment
docker compose build
docker compose up -d
```

## 2. Build
```bash
sh scripts/build.sh
```

## 3. Usage
### mapping
```bash
cd build
./mapping {input/dotfile} {input/archfile} {output/mapping/json}
```

### visualizer
```bash
cd python_tools/visualizer

## output mapping image
dot -Tpng {input/dotfile} -o {output/pngfile} # visualize dot file
python3 mapping_visualize_main.py {input/mapping/json} # visualize mapping result
```

### experiment runner
```bash
sh scripts/exec_mapping_experiment.sh
sh scripts/exec_remapper_experiment.sh
```



## 4.test
```bash
cd build
ctest
```
