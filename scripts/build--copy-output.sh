docker cp $(docker ps --format "{{.Names}}"):/dist .
