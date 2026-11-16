// SPDX-FileCopyrightText: 2026 Core Devices LLC
// SPDX-License-Identifier: Apache-2.0

//! Attribute macros of the `pebbleos` crate, which re-exports them.

use proc_macro::{TokenStream, TokenTree};

/// Makes the function the kernel runs as the application's `main()`.
///
/// It takes no arguments and returns nothing or a `pebbleos::Result`.
#[proc_macro_attribute]
pub fn main(attr: TokenStream, item: TokenStream) -> TokenStream {
    if !attr.is_empty() {
        return error("#[pebbleos::main] takes no arguments");
    }
    let Some(name) = fn_name(item.clone()) else {
        return error("#[pebbleos::main] goes on a function");
    };

    let entry: TokenStream = format!(
        r#"
        #[cfg_attr(target_os = "none", unsafe(export_name = "main"))]
        #[cfg_attr(not(target_os = "none"), unsafe(export_name = "pbl_app_main"))]
        extern "C" fn __pbl_main() -> ::core::ffi::c_int {{
            let main: fn() -> _ = {name};
            ::pebbleos::Termination::report(main())
        }}
        "#
    )
    .parse()
    .unwrap();

    let mut out = item;
    out.extend(entry);
    out
}

fn fn_name(item: TokenStream) -> Option<String> {
    let mut tokens = item.into_iter();
    while let Some(token) = tokens.next() {
        if matches!(&token, TokenTree::Ident(i) if i.to_string() == "fn") {
            return match tokens.next() {
                Some(TokenTree::Ident(name)) => Some(name.to_string()),
                _ => None,
            };
        }
    }
    None
}

fn error(msg: &str) -> TokenStream {
    format!("::core::compile_error!({msg:?});").parse().unwrap()
}
