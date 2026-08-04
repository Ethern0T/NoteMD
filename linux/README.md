# NoteMD para Linux (GTK 4)

Implementação nativa compilada em C17 com GTK 4 para Linux.

## Compilar e Executar

A aplicação liga dinamicamente à biblioteca GTK 4 do sistema.

```sh
make
./build/notemd
```

Para instalar no menu de aplicações do utilizador com o ícone oficial:

```sh
make install-user
```

Para criar e instalar o pacote Debian/Ubuntu (`.deb`):

```sh
make deb
sudo apt install ./build/notemd_1.2.0_amd64.deb
```

## Diretório de Notas

Por predefinição, a aplicação usa `~/Documents/NoteMD`. Para especificar um diretório personalizado:

```sh
NOTEMD_NOTES_DIR=/caminho/para/notas ./build/notemd
```

## Testes

```sh
./build/notemd --self-test
./build/notemd --ui-self-test
```
