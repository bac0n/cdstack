# Expand dots to dot-dot component.

```bash
# Extend cd command.
source ~/.bash_sources && alias cd=__cd__
```
Optional:
```bash
# Enable cd @label function.
[[ $(type -ft cdstack) != builtin ]] && {
    enable -f cdstack cdstack || \
    echo "Unable to enable module cdstack."; \
}

# Default entries for cd @label.
declare -A CDSTACK=( \
    [@git]='/opt/repo/git' \
    [@home]="$HOME" \
    [@inc]='/usr/include' \
)
```
## Examples:
```bash
my@box:/usr/local/bin$ cd ...
my@box:/usr$
```
Embedded dots: /usr/local/bin -> /usr/bin -> /bin
```bash
my@box:/etc$ cd /usr/local/bin/.../bin/.../bin
my@box:/bin$
```
Come on, goddammit, dot me!
```bash
my@box:/tmp/0/1/2/3/4/5/6/7/8/9$ cd ...........
my@box:/tmp$
```
Or
```bash
my@box:/tmp/0/1/2/3/4/5/6/7/8/9$ cd +10
my@box:/tmp$ 
```
Option-termination disables dot functions:
```bash
my@box:~$ cd -- [ ... | +N | @ ]
```
If @label is found in `CDSTACK` array, expand to array value:
```bash
my@box:~$ cd @git
my@box:/opt/repo/git$
```
If `$_` is a regular file, will expand shell variable `__last__` to  
dirname of `$_` (adding `unset __last__` late in your `.bashrc` will start every new session with an empty variable):
```bash
my@box:~$ cp file.sh /usr/local/bin/file.sh
my@box:~$ cd _
my@box:/usr/local/bin$
```
bash-completion:
```bash
my@box:~$ cd ...<tab>
my@box:~$ cd ../../
```
```bash
my@box:~$ cd /usr/local/bin/.../bin/.../bin<tab>
my@box:~$ cd /usr/local/bin/../../bin/../../bin/
```
Undo dots:
```bash
my@box:/usr/local/bin$ cd ../../../<esc> <esc>
my@box:/usr/local/bin$ cd ....
```
```bash
my@box:/tmp/0/1/2/3/4/5/6/7/8/9$ cd +10<tab>
my@box:/tmp/0/1/2/3/4/5/6/7/8/9$ cd ../../../../../../../../../
```
```bash
my@box:~$ cp file.sh /usr/local/bin/file.sh
my@box:~$ cd _<tab>
my@box:~$ cd /usr/local/bin/
```
If @label is found in `CDSTACK` array, expand to array value:
```bash
my@box:~$ cd @git<tab>
my@box:~$ cd /opt/repo/git/
```
