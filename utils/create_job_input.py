from __future__ import print_function
import random as random
import numpy as np

from sys import argv

if __name__ == '__main__':
  block_size = ([8,8,8], [8,8,16], [8,16,16], [16,16,16],
                [16,16,32], [16,32,32], [4,4,8], [4,8,8], 
                [8,8,8], [8,8,16], [8,16,16], [16,16,16])
  count = (75, 50, 25, 10, 10, 5, 75, 50, 25, 10, 10, 5)
  core_count = (512, 1024, 2048, 4096, 8192, 16384,
                128, 256, 512, 1024, 2048, 4096)
  core_shapes = ([2,2,4], [2,2,1])
  node_size = (16,4)
  mtype = int(argv[1])
  outer_box = ([1,1,1])

  jobs_des = ("fg-512", "fg-1K", "fg-2K", 
                  "fg-4K", "fg-8K", "fg-16K",
                  "perm-512", "perm-1K", "perm-2K", 
                  "perm-4K", "perm-8K", "perm-16K",
                  "stencil3d-512", "stencil3d-1K", "stencil3d-2K",
                  "stencil3d-4K", "stencil3d-8K", "stencil3d-16K",
              "fft-128", "fft-256", "fft-512",
              "fft-1K", "fft-2K", "fft-4K",
              "kripke-128", "kripke-256", "kripke-512",
              "kripke-1K", "kripke-2K", "kripke-4K",
              "qbox-128", "qbox-256", "qbox-512",
              "qbox-1K", "qbox-2K", "qbox-4K")

  job_iters = (1,1,1,1,1,1,
               1,1,1,1,1,1,
               1,1,1,1,1,1,
               1,1,1,1,1,1,
               1,1,1,1,1,1,
               1,1,1,1,1,1)


  msg_size =   (2000,2000,2000,2000,2000,2000,
                2592000, 2592000, 5184000, 5184000, 10368000, 10368000,
                10368000,10368000,10368000,10368000,10368000, 10368000,
                9540000, 9540000, 6360000, 6360000, 4770000, 4770000,
                0,0,0,0,0,0,
                0,0,0,0,0,0)

  time_calls =   ([ ["SmallMsgs_Setup", 0.000001 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.020 ] ],
                  [ ["SmallMsgs_Setup", 0.000002 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.023 ] ],
                  [ ["SmallMsgs_Setup", 0.000004 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.025 ] ],
                  [ ["SmallMsgs_Setup", 0.000008 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.030 ] ],
                  [ ["SmallMsgs_Setup", 0.00001 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.033 ] ],
                  [ ["SmallMsgs_Setup", 0.000015 ], ["SmallMsgs_SendTime", 0.000001], ["SmallMsgs_Work", 0.036 ] ],
                  [ ["Permuation_Setup", 0.0035 ], ["Permuation_Work", 0.0035] ],
                  [ ["Permuation_Setup", 0.0045 ], ["Permuation_Work", 0.0045] ],
                  [ ["Permuation_Setup", 0.0055 ], ["Permuation_Work", 0.0055] ],
                  [ ["Permuation_Setup", 0.0065 ], ["Permuation_Work", 0.0065] ],
                  [ ["Permuation_Setup", 0.0075 ], ["Permuation_Work", 0.0075] ],
                  [ ["Permuation_Setup", 0.0085 ], ["Permuation_Work", 0.0085] ],
                  [ ["Stencil3D_Setup", 0.001 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["Stencil3D_Setup", 0.0012 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["Stencil3D_Setup", 0.0014 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["Stencil3D_Setup", 0.0016 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["Stencil3D_Setup", 0.0018 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["Stencil3D_Setup", 0.002 ], ["Stencil3D_Work", 0.036 ] ],
                  [ ["FFT_Setup", 0.001], ["FFT_WORK", 0.001] ],
                  [ ["FFT_Setup", 0.001], ["FFT_WORK", 0.001] ],
                  [ ["FFT_Setup", 0.0008], ["FFT_WORK", 0.0008] ],
                  [ ["FFT_Setup", 0.0008], ["FFT_WORK", 0.0008] ],
                  [ ["FFT_Setup", 0.0004], ["FFT_WORK", 0.0004] ],
                  [ ["FFT_Setup", 0.0004], ["FFT_WORK", 0.0004] ],
                  [ ["Kripke_SolveCompute", 0.013 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["Kripke_SolveCompute", 0.013 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["Kripke_SolveCompute", 0.026 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["Kripke_SolveCompute", 0.026 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["Kripke_SolveCompute", 0.052 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["Kripke_SolveCompute", 0.052 ], ["Kripke_SweepCompute", 0.000135 ] ],
                  [ ["QBOX_Setup", 0.001  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.0005 ], ["QBOX_Work1", .005], ["QBOX_Work2", 0.006 ] ],
                  [ ["QBOX_Setup", 0.0015  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.008 ], ["QBOX_Work1", .007], ["QBOX_Work2", 0.008 ] ],
                  [ ["QBOX_Setup", 0.002  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.001 ], ["QBOX_Work1", .009], ["QBOX_Work2", 0.010 ] ],
                  [ ["QBOX_Setup", 0.0025  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.0013 ], ["QBOX_Work1", .010], ["QBOX_Work2", 0.012 ] ],
                  [ ["QBOX_Setup", 0.003  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.0016 ], ["QBOX_Work1", .015], ["QBOX_Work2", 0.018 ] ],
                  [ ["QBOX_Setup", 0.004  ], ["QBOX_OverlapWork", 0], ["QBOX_FFTTime", 0.002 ], ["QBOX_Work1", .018], ["QBOX_Work2", 0.020 ] ])

  jobid = 0
  random.seed(104723)
  f = open('event.txt', 'w')
  for i in range(len(count)):
    if(i < 6):
      s = 0
      bjind = i
      base = 0
    else:
      s = 1
      bjind = i - 6
      base = 18
    for it in range(count[i]):
      whichJob = random.randint(0,2)
      jind = base + whichJob * 6 + bjind
      #print (core_count[i]/node_size[s]," ",end="")
      #print core_count[i], mtype, \
      #  block_size[i][0],  block_size[i][1], block_size[i][2], \
      #  core_shapes[s][0],  core_shapes[s][1], core_shapes[s][2], \
      #  outer_box[0],  outer_box[1], outer_box[2]
      print (jobs_des[jind], argv[2]+"/job"+str(jobid), core_count[i], job_iters[jind])
      if(msg_size[jind] != 0):
        f.write("M "+str(jobid)+" 100000 "+str(msg_size[jind])+"\n")
      for o in range(len(time_calls[jind])):
        f.write("E "+str(jobid)+" "+str(time_calls[jind][o][0])+" "+str(time_calls[jind][o][1])+"\n")
      jobid = jobid + 1

  print ("\n")
  f.close()


