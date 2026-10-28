#if NEBULA_DEV
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace Nebula.Launcher
{
    /// <summary>
    /// The "Developer" tab of Nebula-Dev.exe (the launcher built with NEBULA_DEV, a separate download that is not part of
    /// the public installer). One button takes the newest nebula-*.patch, puts the source folder back on its base commit,
    /// applies the patch, builds the host and the game module, replaces NebulaNative.exe and smg_game.dll next to this
    /// program and starts the game. The output is shown here and kept in dev-last.log.
    /// </summary>
    internal sealed class DevPage
    {
        private const string BuildScript = @"param([string]$Repo)
$ErrorActionPreference = 'Stop'
Set-Location $Repo
. (Join-Path $Repo 'tools\enter_msvc_environment.ps1')
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake).Source }
if (-not (Test-Path 'build\host\build.ninja')) {
    & $cmake -S . -B build\host -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
& $cmake --build build\host --target NebulaNative
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $Repo 'native\build.ps1') -Petari native\petari -Build build\game
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
";

        private readonly TabPage page;
        private readonly LaunchSession session;
        private readonly TextBox repo = new TextBox();
        private readonly TextBox baseCommit = new TextBox();
        private readonly TextBox patchFolders = new TextBox();
        private readonly ComboBox patchList = new ComboBox();
        private readonly List<FileInfo> patches = new List<FileInfo>();
        private readonly CheckBox runAfter = new CheckBox();
        private readonly CheckBox forceApply = new CheckBox();
        private readonly Button apply = new Button();
        private readonly Button rebuild = new Button();
        private readonly Button stop = new Button();
        private readonly TextBox log = new TextBox();
        private CancellationTokenSource cancel;
        private bool busy;
        private string appliedHash = "";

        private static string ConfigPath { get { return Path.Combine(NebulaPaths.DataRoot, "dev.json"); } }
        private static string LogPath { get { return Path.Combine(NebulaPaths.DataRoot, "dev-last.log"); } }

        public DevPage(TabPage page, LaunchSession session)
        {
            this.page = page;
            this.session = session;
            Build();
            LoadConfig();
            RefreshPatch();
        }

        private void Build()
        {
            Row("Source folder", repo, 12);
            Row("Base commit", baseCommit, 48);
            Row("Patch folders", patchFolders, 84);
            page.Controls.Add(new Label { Text = "Patch to apply", Location = new Point(12, 126), Size = new Size(150, 26) });
            patchList.Location = new Point(170, 122);
            patchList.Size = new Size(330, 28);
            patchList.DropDownStyle = ComboBoxStyle.DropDownList;
            page.Controls.Add(patchList);
            var browse = new Button { Text = "Browse…", Location = new Point(508, 120), Size = new Size(100, 32) };
            browse.Click += delegate { BrowsePatch(); };
            page.Controls.Add(browse);
            var refresh = new Button { Text = "Refresh", Location = new Point(616, 120), Size = new Size(94, 32) };
            refresh.Click += delegate { RefreshPatch(); };
            page.Controls.Add(refresh);

            apply.Text = "Apply the selected patch and rebuild";
            apply.Location = new Point(12, 154);
            apply.Size = new Size(300, 36);
            apply.Click += delegate { Start(true); };
            page.Controls.Add(apply);
            rebuild.Text = "Rebuild only";
            rebuild.Location = new Point(320, 154);
            rebuild.Size = new Size(150, 36);
            rebuild.Click += delegate { Start(false); };
            page.Controls.Add(rebuild);
            stop.Text = "Stop";
            stop.Location = new Point(478, 154);
            stop.Size = new Size(110, 36);
            stop.Enabled = false;
            stop.Click += delegate { if (cancel != null) cancel.Cancel(); };
            page.Controls.Add(stop);

            runAfter.Text = "Start the game when done";
            runAfter.Checked = true;
            runAfter.Location = new Point(12, 196);
            runAfter.Size = new Size(330, 26);
            page.Controls.Add(runAfter);
            forceApply.Text = "Apply again even if it is the same patch";
            forceApply.Location = new Point(350, 196);
            forceApply.Size = new Size(370, 26);
            page.Controls.Add(forceApply);

            log.Location = new Point(12, 230);
            log.Size = new Size(700, 280);
            log.Multiline = true;
            log.ReadOnly = true;
            log.ScrollBars = ScrollBars.Both;
            log.WordWrap = false;
            log.Font = new Font("Consolas", 9f);
            page.Controls.Add(log);
        }

        private void Row(string caption, TextBox box, int y)
        {
            page.Controls.Add(new Label { Text = caption, Location = new Point(12, y + 4), Size = new Size(150, 26) });
            box.Location = new Point(170, y);
            box.Size = new Size(540, 28);
            page.Controls.Add(box);
        }

        private void LoadConfig()
        {
            repo.Text = @"C:\dev\nebula";
            baseCommit.Text = "0dd6d99";
            patchFolders.Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads") + @";C:\dev";
            try
            {
                if (!File.Exists(ConfigPath)) return;
                var config = Json.Load(ConfigPath);
                if (!string.IsNullOrEmpty(Json.Str(config, "repo"))) repo.Text = Json.Str(config, "repo");
                if (!string.IsNullOrEmpty(Json.Str(config, "base"))) baseCommit.Text = Json.Str(config, "base");
                if (!string.IsNullOrEmpty(Json.Str(config, "patchFolders"))) patchFolders.Text = Json.Str(config, "patchFolders");
                appliedHash = Json.Str(config, "patchHash") ?? "";
                if (config.ContainsKey("runAfter")) runAfter.Checked = Json.Bool(config, "runAfter");
            }
            catch (Exception) { }
        }

        private void SaveConfig()
        {
            Json.Save(ConfigPath, new Dictionary<string, object>
            {
                { "repo", repo.Text.Trim() }, { "base", baseCommit.Text.Trim() }, { "patchFolders", patchFolders.Text.Trim() },
                { "patchHash", appliedHash }, { "runAfter", runAfter.Checked }
            });
        }

        private FileInfo SelectedPatch()
        {
            int index = patchList.SelectedIndex;
            return index >= 0 && index < patches.Count ? patches[index] : null;
        }

        /// <summary>Lists every nebula-*.patch of the patch folders, newest first, and keeps the current choice when it is still there.</summary>
        private void RefreshPatch()
        {
            string keep = SelectedPatch() != null ? SelectedPatch().FullName : null;
            var found = new List<FileInfo>();
            foreach (string folder in patchFolders.Text.Split(';'))
            {
                string path = folder.Trim();
                if (path.Length == 0 || !Directory.Exists(path)) continue;
                foreach (string file in Directory.GetFiles(path, "nebula-*.patch")) found.Add(new FileInfo(file));
            }
            foreach (FileInfo extra in patches)
                if (extra.Exists && !found.Exists(f => string.Equals(f.FullName, extra.FullName, StringComparison.OrdinalIgnoreCase))) found.Add(extra);
            found.Sort((x, y) => y.LastWriteTime.CompareTo(x.LastWriteTime));
            patches.Clear();
            patches.AddRange(found);
            patchList.Items.Clear();
            foreach (FileInfo patch in patches) patchList.Items.Add(patch.Name + "  (" + patch.LastWriteTime.ToString("g") + ")");
            int select = 0;
            if (keep != null)
                for (int i = 0; i < patches.Count; i++)
                    if (string.Equals(patches[i].FullName, keep, StringComparison.OrdinalIgnoreCase)) select = i;
            if (patches.Count > 0) patchList.SelectedIndex = select;
        }

        private void BrowsePatch()
        {
            using (var dialog = new OpenFileDialog { Title = "Choose a patch", Filter = "Patches|*.patch;*.diff|All files|*.*", CheckFileExists = true })
            {
                if (dialog.ShowDialog(page.FindForm()) != DialogResult.OK) return;
                var chosen = new FileInfo(dialog.FileName);
                if (!patches.Exists(f => string.Equals(f.FullName, chosen.FullName, StringComparison.OrdinalIgnoreCase))) patches.Add(chosen);
                RefreshPatch();
                for (int i = 0; i < patches.Count; i++)
                    if (string.Equals(patches[i].FullName, chosen.FullName, StringComparison.OrdinalIgnoreCase)) patchList.SelectedIndex = i;
            }
        }

        private void Write(string text)
        {
            try
            {
                page.BeginInvoke((Action)delegate
                {
                    log.AppendText(text + "\r\n");
                    try { File.AppendAllText(LogPath, text + "\r\n", Encoding.UTF8); } catch (Exception) { }
                });
            }
            catch (InvalidOperationException) { }
        }

        private void Start(bool applyPatch)
        {
            if (busy) return;
            string source = repo.Text.Trim();
            string baseRef = baseCommit.Text.Trim();
            if (!Directory.Exists(Path.Combine(source, ".git")))
            {
                MessageBox.Show(page.FindForm(), "The source folder is not a git repository: " + source, "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            FileInfo patch = applyPatch ? SelectedPatch() : null;
            if (applyPatch && patch == null)
            {
                MessageBox.Show(page.FindForm(), "No patch is selected. Download the patch, press Refresh (or Browse…), then try again.", "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            // The list is refreshed when the tab is built and by "Refresh"; a patch downloaded later needs a refresh.
            busy = true;
            apply.Enabled = false;
            rebuild.Enabled = false;
            stop.Enabled = true;
            log.Clear();
            try { File.WriteAllText(LogPath, "", Encoding.UTF8); } catch (Exception) { }
            cancel = new CancellationTokenSource();
            CancellationToken token = cancel.Token;
            bool force = forceApply.Checked;
            string target = session.App;
            var thread = new Thread(delegate()
            {
                string error = null;
                string newHash = null;
                try { newHash = Pipeline(source, baseRef, patch, force, target, token); }
                catch (OperationCanceledByUserException) { error = "Stopped."; }
                catch (Exception failure) { error = failure.Message; }
                try { page.BeginInvoke((Action)delegate { Finished(error, newHash); }); }
                catch (InvalidOperationException) { }
            });
            thread.IsBackground = true;
            thread.Start();
        }

        private int Exec(ProcessRunner runner, string file, string[] args, string workdir, CancellationToken token)
        {
            Write("> " + file + " " + ProcessRunner.JoinArguments(args));
            return runner.Run(file, args, workdir, delegate(string line) { Write(line); }, token);
        }

        private static void Require(int code, string what)
        {
            if (code != 0) throw new InvalidOperationException(what + " failed (code " + code + ").");
        }

        private string Pipeline(string source, string baseRef, FileInfo patch, bool force, string target, CancellationToken token)
        {
            string hash = appliedHash;
            using (var runner = new ProcessRunner())
            {
                if (patch != null)
                {
                    hash = FileUtil.Sha256File(patch.FullName);
                    Write("Patch: " + patch.FullName);
                    if (hash == appliedHash && !force)
                        Write("This patch is already applied: building only.");
                    else
                    {
                        // A patch holds all the work since the base commit: start from the base each time.
                        Require(Exec(runner, "git", new[] { "-C", source, "reset", "--hard", baseRef }, source, token), "git reset");
                        Require(Exec(runner, "git", new[] { "-C", source, "clean", "-fd", "-e", "*.patch" }, source, token), "git clean");
                        Require(Exec(runner, "git", new[] { "-C", source, "apply", "--3way", "--whitespace=nowarn", patch.FullName }, source, token), "git apply");
                    }
                }

                Directory.CreateDirectory(NebulaPaths.DataRoot);
                string script = Path.Combine(NebulaPaths.DataRoot, "dev-build.ps1");
                File.WriteAllText(script, BuildScript, new UTF8Encoding(true));
                Require(Exec(runner, "powershell.exe", new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-Repo", source },
                    source, token), "The build");

                string host = Path.Combine(source, "build", "host", "NebulaNative.exe");
                string game = Path.Combine(source, "build", "game", "smg_game.dll");
                if (!File.Exists(host) || !File.Exists(game)) throw new InvalidOperationException("The build did not make NebulaNative.exe and smg_game.dll.");

                // The running game holds both files.
                foreach (Process running in Process.GetProcessesByName("NebulaNative"))
                {
                    Write("Closing the running game.");
                    try { running.Kill(); running.WaitForExit(5000); } catch (Exception) { }
                }
                File.Copy(host, Path.Combine(target, "NebulaNative.exe"), true);
                File.Copy(game, Path.Combine(target, "smg_game.dll"), true);
                Write("Installed in " + target);
            }
            return hash;
        }

        private void Finished(string error, string hash)
        {
            busy = false;
            apply.Enabled = true;
            rebuild.Enabled = true;
            stop.Enabled = false;
            if (error != null)
            {
                Write("ERROR: " + error);
                Write("Send dev-last.log (in " + NebulaPaths.DataRoot + ") if you want me to look.");
                return;
            }
            if (hash != null) appliedHash = hash;
            try { SaveConfig(); } catch (Exception) { }
            Write("Done.");
            if (!runAfter.Checked) return;
            string launchError = session.TryLaunch();
            if (launchError != null) MessageBox.Show(page.FindForm(), launchError, "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
#endif
