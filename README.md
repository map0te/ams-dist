# ams-dist

A distributed cube-n-conquer solver intended for hard combinatorial problems.  
Authors: [Maxim Zhulin](https://map0te.github.io/), [Piyush Jha](https://piyush-j.github.io/), [Zhengyu Li](https://404briannotfound.tech/)

---

### Build

Compile with Make. Requires MPI.

```bash
git clone --recursive https://github.com/map0te/ams-dist.git
cd ams-dist
make
```

### Usage

```bash
ams-dist [OPTION]... ORDER FILE PATH
```
Solve `FILE` with order `ORDER` in the working directory `PATH`.

### Arguments

| Argument | Description |
|--------|-------------|
| `ORDER` | Solver order parameter |
| `FILE` | Input CNF file to solve |
| `PATH` | Working directory for solver execution |

### Options

| Option | Long Option | Description | Default |
|------|------|-------------|--------|
| `-h` | `--help` | show help message | — |
| `-v` | `--verbose` | enable verbose solver status output | Disabled |
| `-t VAL` | `--twarmup VAL` | time before interrupt (seconds) | `10` |
| `-c` | `--no_share_cas` | turn off CAS clause sharing | Disabled |

---
### Example

Generating a Kochen-Specker instance:
```bash
python gen_instance/generate.py <order> 0.5
python gen_instance/generate.py 17 0.5
```

Running the distributed solver
```bash
mpirun -n <ncores> ams-dist <order> <instance> <workspace>
mpirun -n 16 ams-dist 17 ks_17.cnf temp/
```

---
