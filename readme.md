# Demonstration of Portabililty issues in Repast HPC 2.3.1

In a more complex model, portability issues were encouncounted where different results were achiaved when executing on different machines, and/or in singularity containers with the same software versions.

This was trace back to the order of elements (and potentially the selected elements) when using `void selectAgents(int count, std::vector<T*>& selectedAgents, bool remove = false);`

When seeded via `random.seed`, this deterministically produces the same set of agents for a given build on a given platform, but when executed on a different machine (or the same machine but in a singularity container) it returns a different ordering of agents.
This issue was especially prominent when models included the removal and creation of agents.

Sorting (and shuffling to avoid bias) the order of agents based on their Id led to portable behaviour.

## The MWE

This repostiory contains a working example showing this issue. 

The MWE is a trivial repast hpc model, implemented within a single `main.cpp` file for simplicity.
It contains a single type of agent (`Agent`).
During model intitialisation, a poopulation of these agents are generated, with IDs' in ascending order (number controlled by `count.of.agents`).
Agent ID and memory address are output at construction time.

The model schedules a single execution step, during which a number of agents are selected via `selectAgents` into a `std::vector<Agent*>` which are then output to `stdout` with Agent ID and address. The number of agents selected is controlled via the `count.to.select` model property 

RNG is seeded via a properties file (`random.seed`)

Unfortuantely relying on `new` to reliably reproduce this behaviour in a small / minimal example wasn't being reliable, so this MWE includes 3 modes of execution controlled by 2 config options.

Instead placement new is used to allocate agent data at specific locations / in a specific order, with a fixed upper number of agents controlled by the `Model::MAX_AGENTS` cosntexpr.
The `order.reverse` boolean option controls if agents are allocated in ascending or descending order of pointer.

+ If `order.placementnew == 0`
  + agent are allocated using `new` with undefined order of pointers
+ If `order.placementnew == 1` 
    + Agents are allocated into the pre-allocated aligned storage area using `placement new`. 
    + If `order.reverse == 0` agents are assigned pointers in ascening order. I.e. agent ID 0 will have the lowest memory address
    + If `order.reverse == 1` agents are assigned pointers in descending order, I.e. agent ID 0 wiill have the highest memory address

To demonstrate this, 3 model properties files are included:

+ `props/model.new.props` - allocate using `new` 
  + `order.placementnew=0`
+ `props/model.asc.props` - allocate using `placement new` in ascending order
  + `order.placementnew=1`
  + `order.reverse=0`
+ `props/model.asc.props` - allocate using `placement new` in descinding order
  + `order.placementnew=1`
  + `order.reverse=0`

## Requirements

This requires a working `repast_hpc` install, tested with repast HPC 2.3.1.

## Compilation

The included `Makefile` assumes that include directories and library directories for Repast HPC and Boost are on the `LIBRARY_PATH` / `LD_LIBRARY_PATH` / `CPLUS_INCLUDE_PATH` / `PATH` as requried, and that the mpi compiler is available as `mpicxx` / is controlled via the `MPICXX` variable.

Alternatively the makefile can be modified to include paths to these locations using a number of variables in the first section of the makefile. 

The Makefile is also set up to produce an unoptimised debug build. 


```bash
make -j `nproc`
```

## Execution

The generated binary `bin/main` takes 2 arguments, a `config.props` file and a `model.props` file. Sample files are provided for the 3 execution configurations, with the same numeber of agents and a common seed used.

```bash
# Use new for allocation
mpirun -n 1 ./bin/main ./props/config.props ./props/model.new.props
# Use placement new in ascending order
mpirun -n 1 ./bin/main ./props/config.props ./props/model.asc.props
# Use placement new in descending order
mpirun -n 1 ./bin/main ./props/config.props ./props/model.desc.props
```

## Singularity

`singularity` contains a number of singularity definition files to create singularity images with the requried dependincies installed. 

With a working singulairty install, an image can be created from one of the provided definition files (such as `repast-2.3.1.def`) can be built either:

With root/sudo:

```bash
sudo singularity build singularity/repast-2.3.1.sif singularity/repast-2.3.1.def
```

Or with Singularity Fakeroot (if enabled) via:

```bash
singularity build --fakeroot singularity/repast-2.3.1.sif singularity/repast-2.3.1.def
```

To build the binary for use in singularity it is best to first clean any potential local builds then execute maek 

```bash
singularity exec singularity/repast-2.3.1.sif make clean
singularity exec singularity/repast-2.3.1.sif make all -j `nproc`
```

The model can then be executed using `singularity exec`

```bash
# Use new for allocation
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.new.props
# Use placement new in ascending order
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.asc.props
# Use placement new in descending order
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.desc.props
```

## Example output

Executing the binary with the `./props/model.asc.props` and `./props/model.desc.props` files reliably shows that the value of pointers impacts the select agent order.

When using `model.asc.props` pointer addresses are assigned in ascending order. `selectAgents` produces a vector of agents with ID's `[1, 2, 6, 3]`.

```console
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.asc.props
random.seed: 12
count.of.agents: 8
count.to.select: 4
order.placementnew: 1
order.reverse: 0
Model::init
AgentId(0, 0, 0, 0) initalised at 0x7ffe05b7a8d8
AgentId(1, 0, 0, 0) initalised at 0x7ffe05b7a900
AgentId(2, 0, 0, 0) initalised at 0x7ffe05b7a928
AgentId(3, 0, 0, 0) initalised at 0x7ffe05b7a950
AgentId(4, 0, 0, 0) initalised at 0x7ffe05b7a978
AgentId(5, 0, 0, 0) initalised at 0x7ffe05b7a9a0
AgentId(6, 0, 0, 0) initalised at 0x7ffe05b7a9c8
AgentId(7, 0, 0, 0) initalised at 0x7ffe05b7a9f0
selectAndPrint 4/8
AgentId(1, 0, 0, 0) initalised at 0x7ffe05b7a900
AgentId(2, 0, 0, 0) initalised at 0x7ffe05b7a928
AgentId(6, 0, 0, 0) initalised at 0x7ffe05b7a9c8
AgentId(3, 0, 0, 0) initalised at 0x7ffe05b7a950
```

When using `model.asc.props` pointer addresses are assigned in descending order. `selectAgents` produces a vector of agents with ID's `[6, 3, 1, 2]`. This is the same selection of agent IDs' but in a differing order.

```console
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.desc.props
random.seed: 12
count.of.agents: 8
count.to.select: 4
order.placementnew: 1
order.reverse: 1
Model::init
AgentId(0, 0, 0, 0) initalised at 0x7ffdb5681500
AgentId(1, 0, 0, 0) initalised at 0x7ffdb56814d8
AgentId(2, 0, 0, 0) initalised at 0x7ffdb56814b0
AgentId(3, 0, 0, 0) initalised at 0x7ffdb5681488
AgentId(4, 0, 0, 0) initalised at 0x7ffdb5681460
AgentId(5, 0, 0, 0) initalised at 0x7ffdb5681438
AgentId(6, 0, 0, 0) initalised at 0x7ffdb5681410
AgentId(7, 0, 0, 0) initalised at 0x7ffdb56813e8
selectAndPrint 4/8
AgentId(6, 0, 0, 0) initalised at 0x7ffdb5681410
AgentId(3, 0, 0, 0) initalised at 0x7ffdb5681488
AgentId(1, 0, 0, 0) initalised at 0x7ffdb56814d8
AgentId(2, 0, 0, 0) initalised at 0x7ffdb56814b0
```

Executing using the `new` opearator may show differing pointer ordering in some platforms / possibly for larger populations, but this was not reliable to demonstrate, hence the forced poitner ordering via placement new.
In this case, pointers were assigned in increasing order, producing the same ordering as the asceinding order case.

```console
singularity exec singularity/repast-2.3.1.sif mpirun -n 1 ./bin/main ./props/config.props ./props/model.new.props
random.seed: 12
count.of.agents: 8
count.to.select: 4
order.placementnew: 0
order.reverse: 0
Model::init
AgentId(0, 0, 0, 0) initalised at 0x55ff2bc43090
AgentId(1, 0, 0, 0) initalised at 0x55ff2bc44020
AgentId(2, 0, 0, 0) initalised at 0x55ff2bc44070
AgentId(3, 0, 0, 0) initalised at 0x55ff2bc440c0
AgentId(4, 0, 0, 0) initalised at 0x55ff2bc44160
AgentId(5, 0, 0, 0) initalised at 0x55ff2bc44200
AgentId(6, 0, 0, 0) initalised at 0x55ff2bc442a0
AgentId(7, 0, 0, 0) initalised at 0x55ff2bc44340
selectAndPrint 4/8
AgentId(1, 0, 0, 0) initalised at 0x55ff2bc44020
AgentId(2, 0, 0, 0) initalised at 0x55ff2bc44070
AgentId(6, 0, 0, 0) initalised at 0x55ff2bc442a0
AgentId(3, 0, 0, 0) initalised at 0x55ff2bc440c0
```