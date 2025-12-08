import matplotlib.pyplot as plt
import numpy as np

tests = {
    "Equilateral_3": {
        "expected": (1.0, 0.577),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.35936},
            "78:1C:3C:2D:15:D4": {"pos": (1, 1.732), "dist": 1.35936},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 1.4678},
        },
        "estimate (n=3)": (1.07665, 0.621569)
    },

    "Equilateral_4": {
        "expected": (1.0, 0.577),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.33352},
            "78:1C:3C:2D:15:D4": {"pos": (1, 1.732), "dist": 1.25893},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 1.41254},
        },
        "estimate": (1.05425, 0.664464)
    },

    "Right_3": {
        "expected": (1.0, 1.0),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.35936},
            "78:1C:3C:2D:15:D4": {"pos": (0, 3), "dist": 2.51189},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 2.15443},
        },
        "estimate": (1.69843, 1.222)
    },

    "Right_4": {
        "expected": (1.0, 1.0),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.25893},
            "78:1C:3C:2D:15:D4": {"pos": (0, 3), "dist": 1.88365},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 1.99526},
        },
        "estimate": (1.59904, 1.57216)
    },

    "Iso_3": {
        "expected": (1.0, 1.0),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.35936},
            "78:1C:3C:2D:15:D4": {"pos": (1, 3), "dist": 2.51189},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 2.92864},
        },
        "estimate": (2.68228, 1.15047)
    },

    "Iso_4": {
        "expected": (1.0, 1.0),
        "sensors": {
            "00:4B:12:3C:04:B0": {"pos": (2, 0), "dist": 1.49624},
            "78:1C:3C:2D:15:D4": {"pos": (1, 3), "dist": 2.37137},
            "78:1C:3C:E3:AB:CC": {"pos": (0, 0), "dist": 1.6788},
        },
        "estimate": (1.14492, 0.817523)
    },
}


def plot_test(name, test):
    expected = test["expected"]
    estimate = test["estimate"]

    plt.figure(figsize=(7,7))

    # Plot sensors and circles
    for mac, data in test["sensors"].items():
        x, y = data["pos"]
        d = data["dist"]

        plt.scatter(x, y, s=140)
        plt.text(x + 0.05, y + 0.05, mac, fontsize=8)

        # Distance circle
        circle = plt.Circle((x, y), d, fill=False, linestyle='--')
        plt.gca().add_patch(circle)

    # True device position
    plt.scatter(expected[0], expected[1], s=150, marker='*', label="True Position")

    # estimates
    plt.scatter(estimate[0], estimate[1], s=150, marker='x', label=f"LS Estimate")

    plt.title(f"Triangulation Test: {name}")
    plt.xlabel("X Position")
    plt.ylabel("Y Position")
    plt.grid(True)
    plt.axis("equal")
    plt.legend()
    plt.show()

for name, test in tests.items():
    plot_test(name, test)
