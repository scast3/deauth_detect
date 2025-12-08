import numpy as np
import matplotlib.pyplot as plt

# script to take the results from the experiments and plot them
path_loss = 3.0

# ---------------------------------------------------------
# SENSOR DATA
# ---------------------------------------------------------

sensor_positions = {
    "sensor_1": (0.0, 0.0),
    "sensor_2": (2.0, 0.0),
    "sensor_3": (1.0, 1.732),
}

# ---------------------------------------------------------
# DISTANCES
# ---------------------------------------------------------

distances = {
    "sensor_1": 1.4678,
    "sensor_2": 1.35936,
    "sensor_3": 1.35936,
}

# ---------------------------------------------------------
# ESTIMATED LS LOCATION
# ---------------------------------------------------------
estimated_target = np.array([1.07665, 0.621569])


# ---------------------------------------------------------
# PLOTTING
# ---------------------------------------------------------
plt.figure(figsize=(8, 8))

for sensor, (x, y) in sensor_positions.items():
    plt.scatter(x, y, s=100, label=sensor)
    plt.text(x + 0.1, y + 0.1, sensor)

for sensor, d in distances.items():
    (x, y) = sensor_positions[sensor]
    circle = plt.Circle((x, y), d, fill=False, linestyle='--')
    plt.gca().add_patch(circle)

# Plot estimated target
plt.scatter(estimated_target[0], estimated_target[1], s=200, marker='x', label="Estimated Target")
# Actual target
plt.scatter(1.0, 0.577, s=200, marker='.', label="Actual Target")

plt.title(f"Equilateral Configuration with Path Loss Coeff. = {path_loss}")
plt.xlabel("X Coordinate (m)")
plt.ylabel("Y Coordinate (m)")
plt.axis("equal")
plt.grid(True)
plt.legend()
plt.show()
