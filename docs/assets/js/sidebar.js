function toggleSidebar() {
    let html = document.querySelector("html");
    if (html.classList.contains("sidebar-hidden")) {
        html.classList.remove("sidebar-hidden");
        localStorage.removeItem("sidebar-hidden-stored");
    } else {
        html.classList.add("sidebar-hidden");
        localStorage.setItem("sidebar-hidden-stored", true);
    }
}

// TODO don't show on load if screen small
// TODO make load with hidden not jarring
// TODO preserve page-wrap width on show
window.onload = function () {
    let sidebarHidden = localStorage.getItem("sidebar-hidden-stored");
    if (sidebarHidden) {
        toggleSidebar();
    }
}
