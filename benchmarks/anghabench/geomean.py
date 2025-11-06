import csv
import numpy as np
from collections import defaultdict

filename = 'results.csv'  # Replace with your actual filename

data = defaultdict(lambda: defaultdict(float))

with open(filename, 'r') as f:
    reader = csv.reader(f)
    for row in reader:
        file_path = row[0]
        opt_name = row[1]
        size = float(row[2])
        data[file_path][opt_name] = size

# Calculate geometric means for each optimization
opt_sizes = defaultdict(list)

for file_path, opts in data.items():
    for opt_name, size in opts.items():
        opt_sizes[opt_name].append(size)

print("Geometric Mean Sizes:")
print("-" * 50)
geomeans = {}
for opt_name, sizes in sorted(opt_sizes.items()):
    geomean = np.exp(np.mean(np.log(sizes)))
    geomeans[opt_name] = geomean
    print(f"{opt_name:15} {geomean:.2f}")



print("\nPercentage Decrease from Baseline (averaged per-entry):")
print("-" * 50)

pct_decreases = defaultdict(list)

for file_path, opts in data.items():
    if 'baseline' in opts:
        baseline_size = opts['baseline']
        for opt_name, size in opts.items():
            if opt_name != 'baseline':
                pct_decrease = ((baseline_size - size) / baseline_size) * 100
                pct_decreases[opt_name].append(pct_decrease)

for opt_name, pct_list in sorted(pct_decreases.items()):
    avg_pct_decrease = np.mean(pct_list)
    print(f"{opt_name:15} {avg_pct_decrease:+.2f}%")