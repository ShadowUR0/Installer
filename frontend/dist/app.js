const state = {
    status: null,
    selectedIndex: null,
    busy: false,
    acceptedOpenAsar: false,
    updatePromptShown: false,
    autocomplete: [],
    autocompleteIndex: 0,
};

const $ = (id) => document.getElementById(id);
const els = {
    installList: $("installList"),
    emptyState: $("emptyState"),
    customRadio: $("customRadio"),
    customRadioRow: $("customRadioRow"),
    customPath: $("customPath"),
    autocomplete: $("autocomplete"),
    downloadPrefix: $("downloadPrefix"),
    directoryHint: $("directoryHint"),
    filesDir: $("filesDir"),
    installerVersion: $("installerVersion"),
    installedHash: $("installedHash"),
    latestHash: $("latestHash"),
    latestLine: $("latestLine"),
    githubError: $("githubError"),
    installButton: $("installButton"),
    repairButton: $("repairButton"),
    uninstallButton: $("uninstallButton"),
    openAsarButton: $("openAsarButton"),
    openFolderButton: $("openFolderButton"),
    modalBackdrop: $("modalBackdrop"),
    modalTitle: $("modalTitle"),
    modalMessage: $("modalMessage"),
    modalClose: $("modalClose"),
    modalSecondary: $("modalSecondary"),
    busyOverlay: $("busyOverlay"),
    busyText: $("busyText"),
};

let modalPrimaryAction = null;
let modalSecondaryAction = null;

function backend() {
    return window.go?.main?.InstallerApp;
}

function currentInstall() {
    if (state.selectedIndex == null || state.selectedIndex < 0) return null;
    return state.status?.installs?.find((item) => item.index === state.selectedIndex) || null;
}

function customSelected() {
    return state.selectedIndex === -1;
}

function branchLabel(branch) {
    const labels = {
        stable: "Stable",
        ptb: "Ptb",
        canary: "Canary",
        development: "Development",
        dev: "Dev",
    };
    if (labels[branch]) return labels[branch];
    if (!branch) return "Discord";
    return branch.charAt(0).toUpperCase() + branch.slice(1);
}

function decorateLiquidGlass(root = document) {
    for (const element of root.querySelectorAll(".liquid-glass:not([data-liquid-ready])")) {
        element.dataset.liquidReady = "true";

        if (element.tagName === "BUTTON") {
            const content = document.createElement("span");
            content.className = "liquid-content";
            while (element.firstChild) content.appendChild(element.firstChild);
            element.appendChild(content);
        }

        for (const side of ["top", "right", "bottom", "left"]) {
            const edge = document.createElement("span");
            edge.className = `glass-edge ${side}`;
            edge.setAttribute("aria-hidden", "true");
            element.appendChild(edge);
        }

        element.addEventListener("pointermove", (event) => {
            const rect = element.getBoundingClientRect();
            const x = ((event.clientX - rect.left) / Math.max(rect.width, 1)) * 100;
            const y = ((event.clientY - rect.top) / Math.max(rect.height, 1)) * 100;
            element.style.setProperty("--pointer-x", `${x}%`);
            element.style.setProperty("--pointer-y", `${y}%`);
        });
        element.addEventListener("pointerleave", () => {
            element.style.removeProperty("--pointer-x");
            element.style.removeProperty("--pointer-y");
        });
    }
}

function setLiquidButtonText(button, text) {
    const content = button.querySelector(":scope > .liquid-content");
    if (content) {
        content.textContent = text;
        return;
    }
    button.textContent = text;
    delete button.dataset.liquidReady;
    decorateLiquidGlass(button.parentElement || document);
}

function setBusy(busy, text = "Working...") {
    state.busy = busy;
    els.busyText.textContent = text;
    els.busyOverlay.classList.toggle("hidden", !busy);
    updateButtons();
}

function showModal({
    title,
    message,
    primaryLabel = "Ok",
    secondaryLabel = null,
    onPrimary = null,
    onSecondary = null,
}) {
    els.modalTitle.textContent = title;
    els.modalMessage.textContent = message;
    setLiquidButtonText(els.modalClose, primaryLabel);
    els.modalSecondary.classList.toggle("hidden", !secondaryLabel);
    if (secondaryLabel) setLiquidButtonText(els.modalSecondary, secondaryLabel);
    modalPrimaryAction = onPrimary;
    modalSecondaryAction = onSecondary;
    els.modalBackdrop.classList.remove("hidden");
    decorateLiquidGlass(els.modalBackdrop);
}

function closeModal() {
    els.modalBackdrop.classList.add("hidden");
    modalPrimaryAction = null;
    modalSecondaryAction = null;
}

els.modalClose.addEventListener("click", async () => {
    const action = modalPrimaryAction;
    closeModal();
    if (action) await action();
});

els.modalSecondary.addEventListener("click", async () => {
    const action = modalSecondaryAction;
    closeModal();
    if (action) await action();
});

els.modalBackdrop.addEventListener("click", (event) => {
    if (event.target === els.modalBackdrop && els.modalSecondary.classList.contains("hidden")) closeModal();
});

function renderVersions() {
    const status = state.status;
    if (!status) return;

    els.downloadPrefix.textContent = status.devInstall
        ? "Dev Install:"
        : "Vencord Arabic will be downloaded to:";
    els.directoryHint.classList.toggle("hidden", status.devInstall);
    els.filesDir.textContent = status.filesDir || "—";

    const outdated = status.selfOutdated ? " - OUTDATED" : "";
    els.installerVersion.textContent = `${status.installerTag || "Unknown"} (${status.installerHash || "Unknown"})${outdated}`;
    els.installedHash.textContent = status.installedHash || "None";

    els.githubError.classList.add("hidden");
    els.latestLine.classList.remove("hidden");
    if (!status.ready) {
        els.latestHash.textContent = "Checking...";
    } else if (status.githubOk) {
        els.latestHash.textContent = status.latestHash || "Unknown";
    } else {
        els.latestLine.classList.add("hidden");
        els.githubError.textContent = `Failed to fetch Info from GitHub: ${status.githubError || "Unknown error"}`;
        els.githubError.classList.remove("hidden");
        delete els.githubError.dataset.liquidReady;
        decorateLiquidGlass(els.githubError.parentElement);
    }

    if (status.filesDirError) {
        showModal({
            title: `Error: Failed to create ${status.filesDir}`,
            message: `${status.filesDirError}\n\nResolve this error, then restart me!`,
        });
    }
}

function selectInstall(index) {
    state.selectedIndex = index;
    for (const input of document.querySelectorAll('input[name="discord-install"]')) {
        input.checked = Number(input.value) === index;
    }
    if (index === -1 && document.activeElement !== els.customPath) els.customPath.focus();
    updateButtons();
}

function renderInstalls() {
    const installs = state.status?.installs || [];
    els.installList.replaceChildren();
    els.emptyState.classList.toggle("hidden", installs.length !== 0);

    if (state.selectedIndex == null || (state.selectedIndex >= 0 && !installs.some((item) => item.index === state.selectedIndex))) {
        state.selectedIndex = installs.length ? installs[0].index : -1;
    }

    for (const install of installs) {
        const label = document.createElement("label");
        label.className = "radio-row";

        const input = document.createElement("input");
        input.type = "radio";
        input.name = "discord-install";
        input.value = String(install.index);
        input.checked = state.selectedIndex === install.index;
        input.addEventListener("change", () => selectInstall(install.index));

        const dot = document.createElement("span");
        dot.className = "radio-dot";
        dot.setAttribute("aria-hidden", "true");

        const text = document.createElement("span");
        text.className = "radio-text";
        text.textContent = `${branchLabel(install.branch)} - ${install.path}${install.patched ? " [PATCHED]" : ""}`;

        label.append(input, dot, text);
        els.installList.appendChild(label);
    }

    els.customRadio.checked = state.selectedIndex === -1;
    updateButtons();
}

function updateButtons() {
    const detected = currentInstall();
    const hasChoice = state.selectedIndex != null;
    const githubBlocked = state.status?.ready && !state.status?.githubOk;
    const blocked = state.busy || !hasChoice;

    els.installButton.disabled = blocked || githubBlocked;
    els.repairButton.disabled = blocked || githubBlocked;
    els.uninstallButton.disabled = blocked || (detected ? !detected.patched : false);
    els.openAsarButton.disabled = blocked;

    const openAsarText = detected
        ? (detected.openAsar ? "Uninstall OpenAsar" : "Install OpenAsar")
        : "(Un-)Install OpenAsar";
    setLiquidButtonText(els.openAsarButton, openAsarText);
    els.openAsarButton.classList.toggle("removing", Boolean(detected?.openAsar));

    decorateLiquidGlass(document);
}

function maybeShowUpdatePrompt() {
    if (!state.status?.selfOutdated || state.updatePromptShown) return;
    state.updatePromptShown = true;
    showModal({
        title: "Your Installer is outdated!",
        message: "Would you like to update now?\n\nOnce you press Update Now, the new installer will automatically be downloaded. The Installer will reopen once the update is done.",
        primaryLabel: "Update Now",
        secondaryLabel: "Later",
        onPrimary: updateInstaller,
    });
}

function renderStatus(status) {
    state.status = status;
    renderVersions();
    renderInstalls();
    maybeShowUpdatePrompt();
}

async function callBackend(method, ...args) {
    const api = backend();
    if (!api || typeof api[method] !== "function") {
        throw new Error("The Go backend is not ready yet");
    }
    return api[method](...args);
}

async function loadStatus(refresh = false) {
    try {
        const status = await callBackend(refresh ? "Refresh" : "GetStatus");
        renderStatus(status);
        return true;
    } catch {
        return false;
    }
}

function operationArgs() {
    return [state.selectedIndex ?? -1, customSelected() ? els.customPath.value : ""];
}

async function runOperation(method, busyText) {
    if (state.busy || state.selectedIndex == null) return;
    setBusy(true, busyText);
    try {
        const result = await callBackend(method, ...operationArgs());
        if (result?.status) renderStatus(result.status);
        showModal({
            title: result?.title || (result?.ok ? "Success" : "Error"),
            message: result?.message || "",
        });
    } catch (error) {
        showModal({ title: "Oh No :(", message: error?.message || String(error) });
    } finally {
        setBusy(false);
    }
}

async function updateInstaller() {
    setBusy(true, "Updating Installer...");
    try {
        const result = await callBackend("UpdateInstaller");
        if (!result?.ok) {
            showModal({ title: result?.title || "Failed to update self!", message: result?.message || "Unknown error" });
        }
    } catch (error) {
        showModal({ title: "Failed to update self!", message: error?.message || String(error) });
    } finally {
        setBusy(false);
    }
}

els.customRadio.addEventListener("change", () => {
    if (els.customRadio.checked) selectInstall(-1);
});
els.customRadioRow.addEventListener("click", () => selectInstall(-1));
els.customPath.addEventListener("focus", () => {
    if (!customSelected()) selectInstall(-1);
});
els.customPath.addEventListener("input", async () => {
    if (!customSelected()) selectInstall(-1);
    state.autocompleteIndex = 0;
    const value = els.customPath.value;
    if (!value) {
        state.autocomplete = [];
        renderAutocomplete();
        return;
    }
    try {
        state.autocomplete = await callBackend("CompletePath", value);
    } catch {
        state.autocomplete = [];
    }
    renderAutocomplete();
});

els.customPath.addEventListener("keydown", (event) => {
    if (event.key !== "Tab" || state.autocomplete.length === 0) return;
    event.preventDefault();
    const candidate = state.autocomplete[state.autocompleteIndex % state.autocomplete.length];
    state.autocompleteIndex += 1;
    els.customPath.value = candidate;
    els.customPath.setSelectionRange(candidate.length, candidate.length);
});

function renderAutocomplete() {
    els.autocomplete.replaceChildren();
    els.autocomplete.classList.toggle("hidden", state.autocomplete.length === 0);
    for (const candidate of state.autocomplete.slice(0, 8)) {
        const row = document.createElement("button");
        row.type = "button";
        row.textContent = candidate;
        row.addEventListener("mousedown", (event) => event.preventDefault());
        row.addEventListener("click", () => {
            els.customPath.value = candidate;
            state.autocomplete = [];
            renderAutocomplete();
            els.customPath.focus();
        });
        els.autocomplete.appendChild(row);
    }
}

els.installButton.addEventListener("click", () => runOperation("Install", "Installing Vencord Arabic..."));
els.repairButton.addEventListener("click", () => runOperation("Repair", "Reinstalling / Repairing Vencord Arabic..."));
els.uninstallButton.addEventListener("click", () => runOperation("Uninstall", "Uninstalling Vencord Arabic..."));
els.openAsarButton.addEventListener("click", () => {
    const detected = currentInstall();
    if (!state.acceptedOpenAsar && (!detected || !detected.openAsar)) {
        showModal({
            title: "OpenAsar",
            message: "OpenAsar is an open-source alternative of Discord desktop's app.asar.\nVencord is in no way affiliated with OpenAsar.\nYou're installing OpenAsar at your own risk.\n\nTo install OpenAsar, press Accept and click 'Install OpenAsar' again.",
            primaryLabel: "Accept",
            secondaryLabel: "Cancel",
            onPrimary: () => { state.acceptedOpenAsar = true; },
        });
        return;
    }
    runOperation("ToggleOpenAsar", "Updating OpenAsar...");
});

els.openFolderButton.addEventListener("click", async () => {
    try {
        await callBackend("OpenFilesDirectory");
    } catch (error) {
        showModal({ title: "Failed to open directory", message: error?.message || String(error) });
    }
});

async function boot() {
    decorateLiquidGlass(document);
    for (let attempt = 0; attempt < 50; attempt++) {
        if (await loadStatus(false)) break;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }

    const poll = setInterval(async () => {
        await loadStatus(false);
        if (state.status?.ready && (state.status?.selfOutdated || state.status?.installerTag === "Unknown")) clearInterval(poll);
    }, 700);

    setTimeout(() => clearInterval(poll), 12000);
}

boot();
