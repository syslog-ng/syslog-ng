`dbld`: move dbld image cache from DockerHub to GitHub

In 2021, GitHub introduced the GitHub Packages service. Among other
repositories - it provides a standard Docker registry. DBLD can use
such a regsistry, to avoid unnecessary rebuilding of the images.
