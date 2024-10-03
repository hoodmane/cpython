Module.preRun = () => {
    FS.mkdirTree("/lib/");
    FS.mount(NODEFS, {root: __dirname + "/lib/"}, "/lib/")
}
