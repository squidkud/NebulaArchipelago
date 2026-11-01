using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Forms;

namespace Nebula.Launcher
{
    /// <summary>
    /// The first tab of the launcher: (1) your game: the Super Mario Galaxy image, packed once into game.pak by
    /// nebula-import; (2) the Archipelago room, written to archipelago.ini (the file the game reads at start);
    /// (3) the APWorld for Archipelago. Everything the installer used to ask for lives here.
    /// </summary>
    internal sealed class ArchipelagoPage
    {
        private static readonly Regex ImportProgress = new Regex(@"^PROGRESS (\d+) (\d+)$");

        private readonly TabPage page;
        private readonly LaunchSession session;
        private readonly Label gameStatus = new Label();
        private readonly Button chooseImage = new Button();
        private readonly Button chooseFolder = new Button();
        private readonly Button cancelImport = new Button();
        private readonly ProgressBar progress = new ProgressBar();
        private readonly Label progressText = new Label();
        private readonly CheckBox enabled = new CheckBox();
        private readonly TextBox server = new TextBox();
        private readonly TextBox slot = new TextBox();
        private readonly TextBox password = new TextBox();
        private readonly CheckBox showPassword = new CheckBox();
        private readonly Label roomStatus = new Label();
        private CancellationTokenSource importCancel;
        private bool importing;

        /// <summary>Raised after the game was packed.</summary>
        public event Action GameChanged;

        public ArchipelagoPage(TabPage page, LaunchSession session)
        {
            this.page = page;
            this.session = session;
            Build();
            RefreshGame();
            LoadRoom();
        }

        public bool GameReady
        {
            get { return File.Exists(Path.Combine(session.Content, "game.pak")); }
        }

        public bool Importing { get { return importing; } }

        private static string IniPath
        {
            get { return Path.Combine(NebulaPaths.DataRoot, "archipelago.ini"); }
        }

        private Label Heading(string text, int y)
        {
            var label = new Label { Text = text, Location = new Point(12, y), Size = new Size(700, 28), AutoSize = false };
            label.Font = new Font("Segoe UI", 10f, FontStyle.Bold);
            page.Controls.Add(label);
            return label;
        }

        private Label Caption(string text, int x, int y, int width)
        {
            var label = new Label { Text = text, Location = new Point(x, y + 4), Size = new Size(width, 24), AutoSize = false };
            page.Controls.Add(label);
            return label;
        }

        private Button MakeButton(Button button, string text, int x, int y, int width)
        {
            button.Text = text;
            button.Location = new Point(x, y);
            button.Size = new Size(width, 32);
            page.Controls.Add(button);
            return button;
        }

        private void Build()
        {
            Heading("1. Your game", 8);
            gameStatus.Location = new Point(12, 38);
            gameStatus.Size = new Size(700, 44);
            gameStatus.AutoEllipsis = true;
            page.Controls.Add(gameStatus);
            MakeButton(chooseImage, "Choose your game image…", 12, 86, 230).Click += delegate { ChooseImage(); };
            MakeButton(chooseFolder, "Choose a game folder…", 250, 86, 210).Click += delegate { ChooseFolder(); };
            MakeButton(cancelImport, "Cancel", 468, 86, 110).Click += delegate { if (importCancel != null) importCancel.Cancel(); };
            cancelImport.Visible = false;
            progress.Location = new Point(12, 128);
            progress.Size = new Size(700, 20);
            progress.Maximum = 1000;
            progress.Visible = false;
            page.Controls.Add(progress);
            progressText.Location = new Point(12, 152);
            progressText.Size = new Size(700, 24);
            progressText.ForeColor = SystemColors.GrayText;
            page.Controls.Add(progressText);

            Heading("2. Archipelago room", 184);
            enabled.Text = "Connect to Archipelago when the game starts";
            enabled.Location = new Point(12, 216);
            enabled.Size = new Size(500, 26);
            enabled.Checked = true;
            page.Controls.Add(enabled);
            Caption("Server address", 12, 250, 180);
            server.Location = new Point(200, 250);
            server.Size = new Size(300, 28);
            page.Controls.Add(server);
            var serverHint = Caption("for example archipelago.gg:38281", 510, 250, 210);
            serverHint.ForeColor = SystemColors.GrayText;
            Caption("Player name (slot)", 12, 288, 180);
            slot.Location = new Point(200, 288);
            slot.Size = new Size(300, 28);
            page.Controls.Add(slot);
            var slotHint = Caption("the \"name:\" of your YAML", 510, 288, 210);
            slotHint.ForeColor = SystemColors.GrayText;
            Caption("Room password", 12, 326, 180);
            password.Location = new Point(200, 326);
            password.Size = new Size(300, 28);
            password.UseSystemPasswordChar = true;
            page.Controls.Add(password);
            showPassword.Text = "Show";
            showPassword.Location = new Point(510, 328);
            showPassword.Size = new Size(120, 24);
            showPassword.CheckedChanged += delegate { password.UseSystemPasswordChar = !showPassword.Checked; };
            page.Controls.Add(showPassword);
            var save = MakeButton(new Button(), "Save the room", 12, 366, 180);
            save.Click += delegate { SaveRoom(); };
            roomStatus.Location = new Point(200, 370);
            roomStatus.Size = new Size(520, 26);
            roomStatus.ForeColor = SystemColors.GrayText;
            roomStatus.AutoEllipsis = true;
            page.Controls.Add(roomStatus);

            Heading("3. APWorld (for the Archipelago launcher)", 410);
            MakeButton(new Button(), "Open the APWorld folder", 12, 442, 230).Click += delegate { OpenApworldFolder(); };
            MakeButton(new Button(), "Copy it into Archipelago", 250, 442, 230).Click += delegate { CopyApworld(); };
        }

        // ---- The game ----

        public void RefreshGame()
        {
            if (importing) return;
            bool ready = GameReady;
            gameStatus.Text = ready
                ? "Your game is ready: " + session.Content + "\r\nChoose another image to pack it again."
                : "No game yet. Choose your own Super Mario Galaxy (USA, RMGE01, revision 0): a disc image (ISO, RVZ, WBFS...) or an extracted folder. It is only read, never copied or sent.";
            gameStatus.ForeColor = ready ? Color.FromArgb(0, 110, 0) : SystemColors.ControlText;
            progress.Visible = false;
            cancelImport.Visible = false;
            progressText.Text = "";
        }

        private void ChooseImage()
        {
            using (var dialog = new OpenFileDialog
            {
                Title = "Choose your Super Mario Galaxy disc image",
                Filter = "Disc images|*.iso;*.rvz;*.wbfs;*.ciso;*.gcz;*.wia;*.gcm|All files|*.*",
                CheckFileExists = true
            })
            {
                if (dialog.ShowDialog(page.FindForm()) == DialogResult.OK) StartImport(dialog.FileName);
            }
        }

        private void ChooseFolder()
        {
            using (var dialog = new FolderBrowserDialog { Description = "The extracted Super Mario Galaxy folder", ShowNewFolderButton = false })
            {
                if (dialog.ShowDialog(page.FindForm()) == DialogResult.OK) StartImport(dialog.SelectedPath);
            }
        }

        private void SetBusy(bool busy)
        {
            chooseImage.Enabled = !busy;
            chooseFolder.Enabled = !busy;
            cancelImport.Visible = busy;
            progress.Visible = busy;
            if (busy) gameStatus.Text = "Packing your game: it can take 5 to 10 minutes. You can leave this window open.";
        }

        private void StartImport(string input)
        {
            if (importing) return;
            string importer = Path.Combine(session.App, "tools", "nebula-import.exe");
            if (!File.Exists(importer))
            {
                MessageBox.Show(page.FindForm(), "tools\\nebula-import.exe is missing. Install Nebula Archipelago again.", "Nebula Archipelago",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            importing = true;
            SetBusy(true);
            importCancel = new CancellationTokenSource();
            CancellationToken token = importCancel.Token;
            string root = session.Root;
            var thread = new Thread(delegate()
            {
                string error = null;
                bool canceled = false;
                try { error = Import(importer, input, root, token); }
                catch (OperationCanceledByUserException) { canceled = true; }
                catch (Exception failure) { error = failure.Message; }
                try { page.BeginInvoke((Action)delegate { Finished(error, canceled); }); }
                catch (InvalidOperationException) { }
            });
            thread.IsBackground = true;
            thread.Start();
        }

        private void Report(string text, double fraction)
        {
            try
            {
                page.BeginInvoke((Action)delegate
                {
                    if (text != null) progressText.Text = text;
                    if (fraction >= 0)
                    {
                        progress.Style = ProgressBarStyle.Continuous;
                        progress.Value = (int)Math.Max(0, Math.Min(1000, fraction * 1000));
                    }
                    else progress.Style = ProgressBarStyle.Marquee;
                });
            }
            catch (InvalidOperationException) { }
        }

        /// <summary>Runs nebula-import; returns an error message, or null when the game is packed in &lt;root&gt;\content.</summary>
        private string Import(string importer, string input, string root, CancellationToken cancel)
        {
            string fresh = Path.Combine(root, "content-new");
            try
            {
                using (var runner = new ProcessRunner())
                {
                    Report("Checking your game…", -1);
                    var lines = new List<string>();
                    int code = runner.Run(importer, new[] { "identify", input }, Path.GetDirectoryName(importer),
                        delegate(string line) { lines.Add(line); }, cancel);
                    string json = string.Join("\n", lines.ToArray());
                    Dictionary<string, object> identity = null;
                    int brace = json.IndexOf('{');
                    try { if (brace >= 0) identity = Json.Parse(json.Substring(brace)); }
                    catch (Exception) { }
                    if (code != 0 || identity == null || !Json.Bool(identity, "supported"))
                    {
                        string message = identity != null ? Json.Str(identity, "message") : null;
                        if (!string.IsNullOrEmpty(message)) return message;
                        return lines.Count > 0 ? lines[lines.Count - 1] : "This is not a supported Super Mario Galaxy (USA, RMGE01, revision 0).";
                    }

                    if (Directory.Exists(fresh)) FileUtil.DeleteTree(fresh);
                    Directory.CreateDirectory(root);
                    Report("Packing the game files…", 0);
                    code = runner.Run(importer, new[] { "import", input, "--output", fresh }, Path.GetDirectoryName(importer),
                        delegate(string line)
                        {
                            Match match = ImportProgress.Match(line);
                            if (match.Success)
                            {
                                Report(null, double.Parse(match.Groups[1].Value) / Math.Max(1.0, double.Parse(match.Groups[2].Value)));
                                return;
                            }
                            if (line.StartsWith("STEP ") && line.IndexOf(' ', 5) > 0) Report(line.Substring(line.IndexOf(' ', 5) + 1), -1);
                        }, cancel);
                    if (code != 0) return "The game could not be packed (nebula-import stopped with code " + code + ").";
                }
                if (!File.Exists(Path.Combine(fresh, "game.pak"))) return "The game could not be packed (no game.pak was made).";
                Json.Save(Path.Combine(fresh, "content.json"), new Dictionary<string, object>
                {
                    { "formatVersion", 1 }, { "createdUtc", DateTime.UtcNow.ToString("o") }
                });
                string content = Path.Combine(root, "content");
                string old = Path.Combine(root, "content-old");
                if (Directory.Exists(old)) FileUtil.DeleteTree(old);
                if (Directory.Exists(content)) Directory.Move(content, old);
                Directory.Move(fresh, content);
                try { if (Directory.Exists(old)) FileUtil.DeleteTree(old); } catch (Exception) { }
                return null;
            }
            finally
            {
                try { if (Directory.Exists(fresh)) FileUtil.DeleteTree(fresh); } catch (Exception) { }
            }
        }

        private void Finished(string error, bool canceled)
        {
            importing = false;
            SetBusy(false);
            if (error == null && !canceled)
            {
                session.Content = Path.Combine(session.Root, "content");
                RefreshGame();
                progressText.Text = "Done.";
                if (GameChanged != null) GameChanged();
                return;
            }
            RefreshGame();
            if (canceled) { progressText.Text = "Canceled."; return; }
            MessageBox.Show(page.FindForm(), error, "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        // ---- The room (archipelago.ini) ----

        private void LoadRoom()
        {
            string path = IniPath;
            if (!File.Exists(path))
            {
                roomStatus.Text = "No room saved yet.";
                return;
            }
            bool on = true;
            try
            {
                foreach (string raw in File.ReadAllLines(path))
                {
                    string line = raw.Trim();
                    int equals = line.IndexOf('=');
                    if (line.Length == 0 || line[0] == '#' || line[0] == ';' || equals < 0) continue;
                    string key = line.Substring(0, equals).Trim().ToLowerInvariant();
                    string value = line.Substring(equals + 1).Trim();
                    if (key == "server") server.Text = value;
                    else if (key == "slot" || key == "name") slot.Text = value;
                    else if (key == "password") password.Text = value;
                    else if (key == "enabled")
                    {
                        string v = value.ToLowerInvariant();
                        on = !(v == "0" || v == "false" || v == "no" || v == "off");
                    }
                }
            }
            catch (IOException) { roomStatus.Text = "archipelago.ini could not be read."; return; }
            enabled.Checked = on;
            roomStatus.Text = "Saved in " + path;
        }

        /// <summary>Called by "Save" / "Save and play": writes the room when something was typed, without nagging.</summary>
        public void SaveIfFilled()
        {
            if (server.Text.Trim().Length > 0 && slot.Text.Trim().Length > 0 || !enabled.Checked) WriteRoom(false);
        }

        private void SaveRoom() { WriteRoom(true); }

        private void WriteRoom(bool interactive)
        {
            string address = server.Text.Trim();
            string player = slot.Text.Trim();
            if (enabled.Checked && (address.Length == 0 || player.Length == 0))
            {
                if (!interactive) return;
                MessageBox.Show(page.FindForm(), "Fill in the server address and the player name, or untick \"Connect to Archipelago\".",
                    "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            try
            {
                string path = IniPath;
                var keep = new List<string>();
                if (File.Exists(path))
                {
                    foreach (string raw in File.ReadAllLines(path))
                    {
                        string line = raw.Trim();
                        int equals = line.IndexOf('=');
                        string key = equals > 0 && line[0] != '#' && line[0] != ';' ? line.Substring(0, equals).Trim().ToLowerInvariant() : "";
                        if (key == "server" || key == "slot" || key == "name" || key == "password" || key == "enabled") continue;
                        keep.Add(raw);
                    }
                }
                var text = new StringBuilder();
                text.Append("server=").Append(address).Append("\r\n");
                text.Append("slot=").Append(player).Append("\r\n");
                text.Append("password=").Append(password.Text).Append("\r\n");
                if (!enabled.Checked) text.Append("enabled=0\r\n");
                foreach (string other in keep) text.Append(other).Append("\r\n");
                FileUtil.WriteAllTextAtomic(path, text.ToString());
                // A second file with the .txt extension would be read when this one is missing; keep only one.
                string extra = path + ".txt";
                if (File.Exists(extra)) File.Delete(extra);
                roomStatus.Text = "Saved. It applies the next time the game starts.";
            }
            catch (Exception error)
            {
                if (!interactive) return;
                MessageBox.Show(page.FindForm(), "The room could not be saved: " + error.Message, "Nebula Archipelago",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        // ---- The APWorld ----

        private string ApworldFile()
        {
            string folder = Path.Combine(session.App, "apworld");
            if (!Directory.Exists(folder)) return null;
            string[] files = Directory.GetFiles(folder, "*.apworld");
            return files.Length > 0 ? files[0] : null;
        }

        private void OpenApworldFolder()
        {
            string folder = Path.Combine(session.App, "apworld");
            if (!Directory.Exists(folder))
            {
                MessageBox.Show(page.FindForm(), "The APWorld folder is missing. Install Nebula Archipelago again.", "Nebula Archipelago",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            Process.Start("explorer.exe", "\"" + folder + "\"");
        }

        private void CopyApworld()
        {
            string file = ApworldFile();
            if (file == null)
            {
                MessageBox.Show(page.FindForm(), "The APWorld file is missing. Install Nebula Archipelago again.", "Nebula Archipelago",
                    MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            string target = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Archipelago", "custom_worlds");
            bool copied = false;
            if (Directory.Exists(target))
            {
                try { File.Copy(file, Path.Combine(target, Path.GetFileName(file)), true); copied = true; }
                catch (Exception) { }
            }
            if (!copied)
            {
                using (var dialog = new FolderBrowserDialog
                {
                    Description = "Choose the custom_worlds folder of Archipelago (in its installation folder, or C:\\ProgramData\\Archipelago).",
                    ShowNewFolderButton = true
                })
                {
                    if (dialog.ShowDialog(page.FindForm()) != DialogResult.OK) return;
                    target = dialog.SelectedPath;
                    try { File.Copy(file, Path.Combine(target, Path.GetFileName(file)), true); copied = true; }
                    catch (Exception error)
                    {
                        MessageBox.Show(page.FindForm(), "Could not copy the APWorld: " + error.Message, "Nebula Archipelago",
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
            }
            MessageBox.Show(page.FindForm(), "Copied to " + target + ". Restart the Archipelago launcher to see \"Super Mario Galaxy PC Port\".",
                "Nebula Archipelago", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
