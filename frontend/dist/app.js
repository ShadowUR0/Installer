const state = {
    status: null,
    selectedIndex: null,
    busy: false,
};

const $ = (id) => document.getElementById(id);

const els = {
    installList: $("installList"),
    emptyState: $("emptyState"),
    refreshButton: $("refreshButton"),
    installedHash: $("installedHash"),
    latestHash: $("latestHash"),
    filesDir: $("filesDir"),
    selectedState: $("selectedState"),
    selectedName: $("selectedName"),
    selectedPath: $("selectedPath"),
    installButton: $("installButton"),
    repairButton: $("repairButton"),
    uninstallButton: $("uninstallButton"),
    openAsarButton: $("openAsarButton"),
    openFolderButton: $("openFolderButton"),
    connectionDot: $("connectionDot"),
    connectionText: $("connectionText"),
    modalBackdrop: $("modalBackdrop"),
    modalTitle: $("modalTitle"),
    modalMessage: $("modalMessage"),
    modalIcon: $("modalIcon"),
    modalClose: $("modalClose"),
    busyOverlay: $("busyOverlay"),
    busyText: $("busyText"),
};

function backend() {
    return window.go?.main?.InstallerApp;
}

function runtime() {
    return window.runtime;
}

function escapeHtml(value) {
    return String(value ?? "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}

function branchLabel(branch) {
    const labels = {
        stable: "Discord Stable",
        ptb: "Discord PTB",
        canary: "Discord Canary",
        development: "Discord Development",
    };
    return labels[branch] || `Discord ${branch || ""}`.trim();
}

function currentInstall() {
    return state.status?.installs?.find((item) => item.index === state.selectedIndex) || null;
}

function setBusy(busy, text = "جار التنفيذ") {
    state.busy = busy;
    els.busyText.textContent = text;
    els.busyOverlay.classList.toggle("hidden", !busy);
    updateButtons();
}

function showModal(title, message, error = false) {
    els.modalTitle.textContent = title;
    els.modalMessage.textContent = message;
    els.modalIcon.textContent = error ? "!" : "✓";
    els.modalBackdrop.querySelector(".modal").classList.toggle("error", error);
    els.modalBackdrop.classList.remove("hidden");
}

function hideModal() {
    els.modalBackdrop.classList.add("hidden");
}

function updateConnection() {
    const status = state.status;
    els.connectionDot.className = "connection-dot";

    if (!status) {
        els.connectionText.textContent = "جار الاتصال بالـBackend";
        return;
    }
    if (!status.ready) {
        els.connectionText.textContent = "جار فحص احدث اصدار";
        return;
    }
    if (!status.githubOk) {
        els.connectionDot.classList.add("error");
        els.connectionText.textContent = status.githubError || "تعذر الاتصال بخدمة الاصدارات";
        return;
    }

    els.connectionDot.classList.add("online");
    els.connectionText.textContent = "جاهز";
}

function updateVersions() {
    const status = state.status;
    els.installedHash.textContent = status?.installedHash || "—";
    els.latestHash.textContent = !status
        ? "—"
        : !status.ready
            ? "جار الفحص"
            : status.githubOk
                ? (status.latestHash || "—")
                : "غير متاح";
    els.filesDir.textContent = status?.filesDir || "—";
}

function renderInstalls() {
    const installs = state.status?.installs || [];
    els.installList.innerHTML = "";
    els.emptyState.classList.toggle("hidden", installs.length !== 0);

    if (installs.length === 0) {
        state.selectedIndex = null;
        updateSelected();
        return;
    }

    if (!installs.some((item) => item.index === state.selectedIndex)) {
        state.selectedIndex = installs[0].index;
    }

    for (const install of installs) {
        const button = document.createElement("button");
        button.className = `install-card${install.index === state.selectedIndex ? " selected" : ""}`;
        button.dataset.index = install.index;
        button.innerHTML = `
            <div class="install-card-top">
                <strong>${escapeHtml(branchLabel(install.branch))}</strong>
                ${install.patched ? '<span class="patched-dot">Vencord مثبت</span>' : '<span class="status-badge">غير معدل</span>'}
            </div>
            <small>${escapeHtml(install.path)}</small>
        `;
        button.addEventListener("click", () => {
            state.selectedIndex = install.index;
            renderInstalls();
            updateSelected();
        });
        els.installList.appendChild(button);
    }

    updateSelected();
}

function updateSelected() {
    const install = currentInstall();
    if (!install) {
        els.selectedState.textContent = "لم تحدد نسخة";
        els.selectedState.classList.remove("ready");
        els.selectedName.textContent = "Discord";
        els.selectedPath.textContent = "اختر نسخة من القائمة";
        els.openAsarButton.querySelector("span:last-child").textContent = "OpenAsar";
        updateButtons();
        return;
    }

    els.selectedName.textContent = branchLabel(install.branch);
    els.selectedPath.textContent = install.path;
    els.selectedState.textContent = install.patched ? "Vencord Arabic مثبت" : "جاهز للتثبيت";
    els.selectedState.classList.toggle("ready", install.patched);
    els.openAsarButton.querySelector("span:last-child").textContent = install.openAsar ? "ازالة OpenAsar" : "تثبيت OpenAsar";
    updateButtons();
}

function updateButtons() {
    const install = currentInstall();
    const blocked = state.busy || !install;
    const githubUnavailable = state.status?.ready && !state.status?.githubOk;

    els.installButton.disabled = blocked || githubUnavailable;
    els.repairButton.disabled = blocked || githubUnavailable;
    els.uninstallButton.disabled = blocked || !install?.patched;
    els.openAsarButton.disabled = blocked;
    els.refreshButton.disabled = state.busy;
}

function renderStatus(status) {
    state.status = status;
    updateVersions();
    updateConnection();
    renderInstalls();
    updateButtons();

    if (status?.filesDirError) {
        showModal("تعذر تجهيز مجلد Vencord", status.filesDirError, true);
    }
}

async function callBackend(method, ...args) {
    const api = backend();
    if (!api || typeof api[method] !== "function") {
        throw new Error("لم تصبح واجهة Go جاهزة بعد");
    }
    return api[method](...args);
}

async function loadStatus(refresh = false) {
    try {
        const status = await callBackend(refresh ? "Refresh" : "GetStatus");
        renderStatus(status);
        return true;
    } catch (error) {
        els.connectionDot.className = "connection-dot error";
        els.connectionText.textContent = "تعذر الاتصال بالـBackend";
        return false;
    }
}

async function runOperation(method, busyText) {
    const install = currentInstall();
    if (!install || state.busy) return;

    setBusy(true, busyText);
    try {
        const result = await callBackend(method, install.index);
        if (result?.status) renderStatus(result.status);
        showModal(result?.title || (result?.ok ? "تم" : "حدث خطا"), result?.message || "", !result?.ok);
    } catch (error) {
        showModal("حدث خطا", error?.message || String(error), true);
    } finally {
        setBusy(false);
    }
}

els.refreshButton.addEventListener("click", async () => {
    els.refreshButton.textContent = "…";
    await loadStatus(true);
    els.refreshButton.textContent = "↻";
});

els.installButton.addEventListener("click", () => runOperation("Install", "جار تثبيت Vencord Arabic"));
els.repairButton.addEventListener("click", () => runOperation("Repair", "جار تحديث Vencord Arabic واصلاحه"));
els.uninstallButton.addEventListener("click", () => runOperation("Uninstall", "جار الغاء تثبيت Vencord Arabic"));
els.openAsarButton.addEventListener("click", () => runOperation("ToggleOpenAsar", "جار تحديث OpenAsar"));

els.openFolderButton.addEventListener("click", async () => {
    try {
        await callBackend("OpenFilesDirectory");
    } catch (error) {
        showModal("تعذر فتح المجلد", error?.message || String(error), true);
    }
});

els.modalClose.addEventListener("click", hideModal);
els.modalBackdrop.addEventListener("click", (event) => {
    if (event.target === els.modalBackdrop) hideModal();
});

for (const button of document.querySelectorAll("[data-action]")) {
    button.addEventListener("click", () => {
        const action = button.dataset.action;
        const rt = runtime();
        if (action === "minimise") rt?.WindowMinimise?.();
        if (action === "close") rt?.Quit?.();
    });
}

async function boot() {
    for (let attempt = 0; attempt < 50; attempt++) {
        if (await loadStatus(false)) break;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const poll = setInterval(async () => {
        if (state.status?.ready) {
            clearInterval(poll);
            return;
        }
        await loadStatus(false);
    }, 650);
}

boot();
