# Reprodukcja B3WHEEL-STEER-01 — source localization v2

Wymagany source: `380378ef52df980c62d2db87641dec2aa07e97ae`.

```bash
cmake --preset linux-research
cmake --build --preset linux-research --target jv_steering_release_probe -j2
rm -rf /tmp/jv-steer-v2-a /tmp/jv-steer-v2-b
mkdir -p /tmp/jv-steer-v2-a /tmp/jv-steer-v2-b
./build-research/bin/jv_steering_release_probe --dense-sweep --trace-dir /tmp/jv-steer-v2-a > /tmp/jv-steer-v2-a.summary.txt
./build-research/bin/jv_steering_release_probe --dense-sweep --trace-dir /tmp/jv-steer-v2-b > /tmp/jv-steer-v2-b.summary.txt
cmp /tmp/jv-steer-v2-a.summary.txt /tmp/jv-steer-v2-b.summary.txt
(cd /tmp/jv-steer-v2-a && sha256sum *.csv | sort) > /tmp/a.sha
(cd /tmp/jv-steer-v2-b && sha256sum *.csv | sort) > /tmp/b.sha
cmp /tmp/a.sha /tmp/b.sha
```

`--trace-dir` musi wskazywać pusty katalog. Podsumowanie należy przekierować poza
katalog trace; inaczej samo przekierowanie utworzy plik przed startem sondy i
prawidłowo uruchomi negatywną bramkę `trace_directory_not_empty`.
