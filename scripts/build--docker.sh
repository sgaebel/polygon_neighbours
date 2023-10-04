python3.10 -m build --outdir /dist
python3.10 -m build --sdist --outdir /dist
auditwheel repair /output/*whl -w /dist
yes | rm /dist/*-linux_*
echo "Waiting to allow for output being copied to host."
sleep 30
echo "Exiting container."
