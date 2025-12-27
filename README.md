# UBSIM Memory Demo

## Build

```sh
make
```

## Run the memory demo via the manager

1. Build the binaries with `make`.
2. Launch the manager with your active Python virtual environment:

```sh
python -m ubsim_manager ./topos/basicmem-memstim.json
```

The manager creates run-scoped state under `./tmp/ubsim-run-<timestamp>-<pid>`
inside the repository root and keeps it for post-run inspection.

## Notes

* The manager provisions the shared memory queues and passes connection
  metadata to each simulator via `UBSIM_MANAGER_PORTS`.
* `basicmem` and `memstim` now attach to manager-created channels directly
  without any listener/connecter branching.
* Each run directory stores per-simulator command lines (`*.cmd`) and logs
  (`*.stdout.log`/`*.stderr.log`) so you can replay runs without the manager.
