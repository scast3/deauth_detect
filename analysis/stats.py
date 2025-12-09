from plotter import tests
import math
import matplotlib.pyplot as plt
from statistics import mean, median

def dist(a, b):
    return math.sqrt((a[0] - b[0])**2 + (a[1] - b[1])**2)

results = [] 

for name, data in tests.items():
    expected = data["expected"]
    estimate = data["estimate"]

    config = name

    n_value = 3 if "3" in name else 4

    error = dist(expected, estimate)

    results.append({
        "name": name,
        "config": config,
        "n": n_value,
        "expected": expected,
        "estimate": estimate,
        "error": error
    })

names = [r["name"] for r in results]
errors = [r["error"] for r in results]
n_values = [r["n"] for r in results]
configs = [r["config"] for r in results]

summary_by_config = {}

print("\n==============================")
print("   LOCALIZATION ERROR BY CONFIGURATION")
print("==============================\n")

for cfg in configs:
    cfg_errors = [r["error"] for r in results if r["config"] == cfg]

    if not cfg_errors:
        continue

    summary_by_config[cfg] = {
        "mean_error": mean(cfg_errors),
        "median_error": median(cfg_errors),
        "min_error": min(cfg_errors),
        "max_error": max(cfg_errors)
    }

    print(f"{cfg}:")
    print(f"  Mean Error   = {mean(cfg_errors):.4f} m")
    print(f"  Median Error = {median(cfg_errors):.4f} m")
    print(f"  Min Error    = {min(cfg_errors):.4f} m")
    print(f"  Max Error    = {max(cfg_errors):.4f} m\n")

summary_by_n = {}

print("\n==============================")
print("   LOCALIZATION ERROR BY PATH LOSS EXPONENT (n)")
print("==============================\n")

for n_val in [3, 4]:
    n_errors = [r["error"] for r in results if r["n"] == n_val]

    summary_by_n[n_val] = {
        "mean_error": mean(n_errors),
        "median_error": median(n_errors),
        "min_error": min(n_errors),
        "max_error": max(n_errors)
    }

    print(f"n = {n_val}:")
    print(f"  Mean Error   = {mean(n_errors):.4f} m")
    print(f"  Median Error = {median(n_errors):.4f} m")
    print(f"  Min Error    = {min(n_errors):.4f} m")
    print(f"  Max Error    = {max(n_errors):.4f} m\n")

print("\n==============================")
print("   FULL TEST RESULTS")
print("==============================\n")

for r in results:
    print(f"{r['name']:15s} | {r['config']:11s} | n={r['n']} | "
          f"Error={r['error']:.4f} m")
    
# ------------------------------------------------------------
# BOX PLOTS
# ------------------------------------------------------------
n3_errors = [r["error"] for r in results if r["n"] == 3]
n4_errors = [r["error"] for r in results if r["n"] == 4]

equilateral_errors = [r["error"] for r in results if "Equilateral" in r["config"]]
right_errors = [r["error"] for r in results if "Right" in r["config"]]
iso_errors = [r["error"] for r in results if "Iso" in r["config"]]

plt.figure(figsize=(8, 6))
plt.boxplot([equilateral_errors, right_errors, iso_errors],
            labels=["Equilateral", "Right", "Isosceles"])
plt.ylabel("Localization Error (m)")
plt.title("Error Distribution by Sensor Configuration")
plt.tight_layout()
plt.show()

plt.figure(figsize=(8, 6))
plt.boxplot([n3_errors, n4_errors], labels=["n=3", "n=4"])
plt.ylabel("Localization Error (m)")
plt.title("Error Distribution: n=3 vs n=4")
plt.tight_layout()
plt.show()
