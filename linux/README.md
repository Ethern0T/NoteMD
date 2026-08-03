# NoteMD para Linux (GTK 4)

Implementação nativa compilada em C17 com GTK 4, 

## Executar

ompilador C. A aplicação liga dinamicamente à biblioteca GTK 4
do sistema e não necessita dos headers GTK para esta primeira etapa.

```sh
make -C linux
./linux/build/notemd
```

Para instalar no menu de aplicações do utilizador, com o ícone oficial:

```sh
make -C linux install-user
```

Para criar um pacote Debian/Ubuntu instalável:

```sh
make -C linux deb
sudo apt install ./linux/build/notemd_1.2.0_amd64.deb
```

Em Fedora:

```sh
sudo dnf install gtk4
make -C linux
./linux/build/notemd
```

Por predefinição, a aplicação usa `~/Documents/NoteMD`. Para escolher a mesma
pasta usada no macOS:

```sh
NOTEMD_NOTES_DIR=/caminho/para/notas ./linux/build/notemd
```

## Testes

```sh
./linux/build/notemd --self-test
./linux/build/notemd --export-self-test
```
