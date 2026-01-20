import numpy as np

def load_cmvn(
        filename
    ) -> np.ndarray:
    with open(filename, "r", encoding="utf-8") as f:
        lines = f.readlines()

    means_list = []
    vars_list = []
    for i in range(len(lines)):
        line_item = lines[i].split()
        if line_item[0] == "<AddShift>":
            line_item = lines[i + 1].split()
            if line_item[0] == "<LearnRateCoef>":
                add_shift_line = line_item[3 : (len(line_item) - 1)]
                means_list = list(add_shift_line)
                continue
        elif line_item[0] == "<Rescale>":
            line_item = lines[i + 1].split()
            if line_item[0] == "<LearnRateCoef>":
                rescale_line = line_item[3 : (len(line_item) - 1)]
                vars_list = list(rescale_line)
                continue

    means = np.array(means_list).astype(np.float64).tolist()
    vars = np.array(vars_list).astype(np.float64).tolist()
    return means, vars

neg_mean, inv_std = load_cmvn("am.mvn")
print(f"neg_mean: {neg_mean}")
print(f"inv_std: {inv_std}")