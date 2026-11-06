//! Windows: put Harold's icon and the app's name into the exe, so Explorer,
//! the taskbar, Task Manager and the Start menu show them.

fn main() {
    println!("cargo:rerun-if-changed=assets/duckwalk.ico");
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let mut res = winresource::WindowsResource::new();
        res.set_icon("assets/duckwalk.ico")
            .set("FileDescription", "Duckwalk")
            .set("ProductName", "Duckwalk");
        res.compile().expect("Harold's icon");
    }
}
