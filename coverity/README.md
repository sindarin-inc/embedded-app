# Using Coverity

## Coverity inside Docker

Coverity's `cov-build` doesn't work on arm64, so an alternate way to use it is by running it inside QEMU-emulated x86_64 Docker.

### Connect to GitHub Container Registry

You can pull down John's pre-built Docker image from GHCR.

Create a token [here](https://github.com/settings/tokens) with at least `read:packages`, `write:packages`, and `delete:packages` scopes. Then set the token in your `~/.zshrc`, `~/.profile` or equivalent:

```sh
export GITHUB_TOKEN="YOURGITHUBTOKENHERE"
```

Login to GitHub container registry:

```sh
echo $GITHUB_TOKEN | docker login ghcr.io -u YOURGITHUBUSERNAMEHERE --password-stdin
```

Now any Docker commands you use with `ghcr.io/sindarin-inc/coverity` should download the image automatically as necessary.

### Run `cov-build` in Docker

Run this from the root directory of the embedded-app project. This takes ~20 minutes to build.

```sh
pio run -t clean
docker run -v $PWD:/project --platform linux/amd64 -it --rm ghcr.io/sindarin-inc/coverity:2022.3.0 cov-build --dir coverity/cov-emit pio run
```

### Now you can analyze and emit errors

```sh
docker run -v $PWD:/project --platform linux/amd64 -it --rm ghcr.io/sindarin-inc/coverity:2022.3.0 bash -c "cov-analyze --dir coverity/cov-emit && cov-format-errors -x --dir coverity/cov-emit --html-output coverity/out"
open coverity/out/index.html
```

Alternately the local versions of `cov-analyze` and `cov-format-errors` work just fine on arm64. Though you'll need to use `cov-manage-emit add-other-hosts` first:

```sh
cov-manage-emit --dir coverity/cov-emit add-other-hosts
cov-analyze --dir coverity/cov-emit
cov-format-errors -x --dir coverity/cov-emit --html-output coverity/out
open coverity/out/index.html
```

### Re-build the Coverity Docker image

You only need to do this if the license changes or if you want to upgrade the version of Coverity. You'll need `cov-analysis-linux64-2022.3.0.tar.gz` and `license.dat` (from the Synopsys site) to be in this directory. From this directory run:

```sh
docker buildx build --platform linux/amd64 -t ghcr.io/sindarin-inc/coverity:2022.3.0 .
docker push ghcr.io/sindarin-inc/coverity:2022.3.0
```
