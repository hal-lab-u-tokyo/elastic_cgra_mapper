# Elastic CGRA Mapper
mapping tool for Elastic CGRA


## requirement (currently confirmed to work)
GCC >= 8.5.0 

gurobi = 9.1.1

cmake >= 3.20.2

## 1.setup
- clone this repository to your $HOME directory
- place the gurobi WLS license file (gurobi.lic) to license folder

```docker
cd environment 
docker compose build
docker compose up -d
```

## 2.usage
### mapping
```bash
cd build
./mapping
```

### visualizer 
```bash
cd visualizer
dot -Tpng {input/dotfile} -o {output/pngfile} # visualize dot file
python3 main.py {input/mapping/json} # visualize mapping result
```

### experiment runner
```bash
cd experiment_runner
python3 mapping_runner.py {nunber_of_process}
```

## 3.build
```bash
mkdir build && cd build
cmake ..
make -j $(nproc)
```

## 4.test
```bash
cd build
ctest
```


