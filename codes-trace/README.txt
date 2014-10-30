How to make:

1 - cd bigsim
2 - Modify the charm path in the Makefile
3 - make
4- cd ..
5 - Modifty the charm & codes & ross paths in the Makefile
6 - make

How to run:

1 - Make sure your trace files in this directory. 
2 - Run serial: ./modelnet_test_bigsim --sync=1 -- conf/modelnet-test.conf 
2 - Run parallel: mpiexec -n 2 ./modelnet_test_bigsim --sync=2 -- conf/modelnet-test.conf 


