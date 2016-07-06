import matplotlib.pyplot as plt

import DRS4Wrapper

import numpy as np

drs = DRS4Wrapper.DRS4Wrapper()


if drs.initBoard() == 1:

    values1 = np.zeros(100)
    values2 = np.zeros(100)

    for i in range(100):
        while True:
            drs.record()
            values1[i] = drs.measureDelay(1, 2, 0, 0, 250, 250)


    print(values1.mean())
    print(values1.std())

    
    fig = plt.figure()
    ax1 = fig.add_subplot(111)

    ax1.scatter(values1, values2)

    plt.show()

