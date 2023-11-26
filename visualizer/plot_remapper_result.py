from matplotlib import pyplot as plt

DP_time = [0.001188,0.001256,0.001998,0.002838, 0.003237,0.004108, 0.004758, 0.005486, 0.006721, 0.008303, 0.009484, 0.010895,0.011958, 0.013988, 0.01521]
DP_parallel = [6,6,10,13,15,19,24,25,32,36,42,46,54,59,66]

Naive_time = [0.001369,0.001387,0.002374,0.002033,0.003861,0.004587,0.005601,0.007419,0.007343, 0.007927, 0.007083,0.01236, 0.013897, 0.017071, 0.020487]
Naive_parallel = [6,6,10,12,15,20,22,26,32,34,40,46,54,57,63]

if __name__ == "__main__": 
  DP_util = []
  Naive_util = []

  for i in range(6,21):
    DP_util.append(19 * DP_parallel[i-6] / (i*i*4))
    Naive_util.append(19 * Naive_parallel[i-6] / (i*i*4))

  fig, ax = plt.subplots()
  ax.set_xlabel("n")
  ax.set_ylabel("utilization (%)")
  ax.plot(range(6,21), DP_util, label="DP")
  ax.plot(range(6,21), Naive_util, label="Naive")
  ax.legend()
  fig.savefig("./output/remapper/utilization.png")


  fig, ax = plt.subplots()
  ax.set_xlabel("n")
  ax.set_ylabel("remap time (s)")
  ax.plot(range(6,21), DP_time, label="DP")
  ax.plot(range(6,21), Naive_time, label="Naive")
  ax.legend()
  fig.savefig("./output/remapper/remapper_time.png")
