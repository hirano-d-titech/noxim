import matplotlib.pyplot as plt

# データの定義
x = [0.04 * i for i in range(1, 13)]  # [0.02, 0.04, ..., 0.2]

x_name = ["RAW", "HAMMING"]

err = [
    [
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    ],
    [
        3 / 6303,
        8 / 12459,
        7 / 18767,
        13 / 25158,
        9 / 27793,
        11 / 27613,
        16 / 27807,
        18 / 28029,
        8 / 27724,
        14 / 27836,
        16 / 28049,
        10 / 27704
    ]
]

fdp = [
    [
        0.00015361,
        0.000390046,
        0.000314432,
        0.000427151,
        0.000341096,
        0.000388229,
        0.000247113,
        0.000336674,
        0.000308285,
        0.00023232,
        0.00030822,
        0.000326133
    ],
    [
        0.000471698,
        0.000714683,
        0.000475084,
        0.000513063,
        0.000321016,
        0.00039469,
        0.000570166,
        0.000636515,
        0.000285724,
        0.000570064,
        0.000670762,
        0
    ]
]

gad = [
    [
        8.74911,
        10.6934,
        13.1818,
        16.853,
        22.6405,
        33.7504,
        63.3218,
        466.412,
        4963.84,
        9459.73,
        12939.4,
        15511.3
    ],
    [
        12.3243,
        20.7036,
        43.4021,
        169.954,
        6055.68,
        12656.1,
        18658.7,
        22507.1,
        25723.2,
        28019,
        29588.4,
        31698.5
    ]
]

nt = [
    [
        0.521687,
        1.02543,
        1.52639,
        2.06051,
        2.58313,
        3.09279,
        3.56214,
        4.04193,
        4.15552,
        4.13447,
        4.15478,
        4.17284
    ],
    [
        0.891172,
        1.76241,
        2.65439,
        3.55762,
        3.93029,
        3.90467,
        3.93218,
        3.96336,
        3.92035,
        3.9366,
        3.96691,
        3.91773
    ]
]

# プロットの作成
fig, axes = plt.subplots(1, 2, figsize=(16, 6), sharex=True, sharey=False)

plt.suptitle("delay and through put")

# グラフ1の描画
for idx, series in enumerate(gad, start=0):
    axes[0].plot(x, series, marker='o', label=x_name[idx])
axes[0].set_xlabel('global average delay (cycles)')
axes[0].set_ylabel('average delay')
axes[0].set_ylim(bottom=0,top=1000)
axes[0].legend()
axes[0].grid(True)

# グラフ2の描画
for idx, series in enumerate(nt, start=0):
    axes[1].plot(x, series, marker='o', label=x_name[idx])
axes[1].set_xlabel('Network throughput (flits/cycle)')
axes[1].set_ylabel('through put')
axes[1].legend()
axes[1].grid(True)


# レイアウトの調整
plt.tight_layout(rect=[0, 0.03, 1, 0.95]) 

# グラフの表示
plt.show()