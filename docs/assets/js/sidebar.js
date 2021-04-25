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

// hide sidebar before it is displayed to prevent flashing
function toggleSidebarOnLoad() {
    let sidebarHidden = localStorage.getItem("sidebar-hidden-stored");
    if (sidebarHidden || document.body.clientWidth < 1080) {
        toggleSidebar();
    }
}
