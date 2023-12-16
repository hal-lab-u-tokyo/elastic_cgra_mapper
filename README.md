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

## output mapping image
dot -Tpng {input/dotfile} -o {output/pngfile} # visualize dot file
python3 main.py {input/mapping/json} # visualize mapping result

## output remapper evaluation 
python3 output_mapping_result_to_csv.py config/remapper_config.json # output csv cache data for plot
python3 plot_compare_benchmark.py config/remapper_config.json # output images for benchmark comparison
python3 plot_compare_cgra_size.py config/remapper_config.json # output images for cgra size comparison
```

### experiment runner
```bash
cd experiment_runner
python3 mapping_runner.py {nunber_of_process}
```

## 3.build
```bash
mkdir build && cd build
cmake .. -GNinja && ninja
## for debug
# cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug && ninja
```

## 4.test
```bash
cd build
ctest
```


