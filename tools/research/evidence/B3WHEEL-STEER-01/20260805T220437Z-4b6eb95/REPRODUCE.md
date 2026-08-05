# Reprodukcja B3WHEEL-STEER-01

Z czystego checkoutu commita `4b6eb953b64660b3eec43b66efcc3343635fa66f`:

```bash
cmake --preset linux-research
cmake --build --preset linux-research -j2
rm -rf /tmp/jv-steering-release-a /tmp/jv-steering-release-b
mkdir -p /tmp/jv-steering-release-a /tmp/jv-steering-release-b
build-research/bin/jv_steering_release_probe --trace-dir /tmp/jv-steering-release-a > /tmp/jv-steering-release-a.summary
build-research/bin/jv_steering_release_probe --trace-dir /tmp/jv-steering-release-b > /tmp/jv-steering-release-b.summary
cmp /tmp/jv-steering-release-a.summary /tmp/jv-steering-release-b.summary
for f in /tmp/jv-steering-release-a/*.csv; do cmp "$f" "/tmp/jv-steering-release-b/$(basename "$f")"; done
```

Katalog trace musi być pusty. Instrument celowo odmawia zapisu do niepustego katalogu, aby stare pliki nie mogły wejść do nowego evidence.
