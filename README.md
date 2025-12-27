# UBSIM Memory Demo

## Build

```sh
make
```

## Run the memory demo via the manager

1. Build the binaries with `make`.
2. Launch the manager with your active Python virtual environment:

```sh
python -m ubsim_manager ./sims/mem/topos/basicmem-memstim.json
```

The manager creates run-scoped state under `./tmp/ubsim-run-<timestamp>-<pid>`
inside the repository root and cleans it up automatically when the run ends.

## Notes

* The manager provisions the shared memory queues and passes connection
  metadata to each simulator via `UBSIM_MANAGER_PORTS`.
* `basicmem` and `memstim` now attach to manager-created channels directly
  without any listener/connecter branching.
