using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace Nebula.Launcher
{
    /// <summary>
    /// The "Updates" tab: the installed version, a "Check for updates" button and, when GitHub has a newer release,
    /// its notes and an "Install" button. The launcher checks by itself each time it opens (SettingsForm calls
    /// CheckAsync), unless "Check for updates when Nebula Archipelago opens" is unticked.
    /// </summary>
    internal sealed class UpdatesPage
    {
        private readonly TabPage page;
        private readonly LaunchSession session;
        private readonly Label version = new Label();
        private readonly Label status = new Label();
        private readonly Button check = new Button();
        private readonly Button install = new Button();
        private readonly TextBox notes = new TextBox();
        private ReleaseInfo found;
        private bool checking;
        private bool lastAutomatic;

        /// <summary>True when the last check was the one made at opening (no button pressed).</summary>
        public bool Automatic { get { return lastAutomatic; } }

        /// <summary>Raised on the interface thread when a newer release was found.</summary>
        public event Action<ReleaseInfo> UpdateFound;

        public UpdatesPage(TabPage page, LaunchSession session, CheckBox askUpdates)
        {
            this.page = page;
            this.session = session;
            version.Text = "Installed version: Nebula Archipelago " + BuildInfo.Version;
            version.Location = new Point(12, 14);
            version.Size = new Size(700, 28);
            page.Controls.Add(version);

            check.Text = "Check for updates";
            check.Location = new Point(12, 52);
            check.Size = new Size(200, 34);
            check.Click += delegate { CheckAsync(false); };
            page.Controls.Add(check);

            install.Text = "Install the update";
            install.Location = new Point(222, 52);
            install.Size = new Size(200, 34);
            install.Enabled = false;
            install.Click += delegate { InstallFound(); };
            page.Controls.Add(install);

            status.Location = new Point(12, 96);
            status.Size = new Size(700, 28);
            page.Controls.Add(status);

            notes.Location = new Point(12, 132);
            notes.Size = new Size(700, 290);
            notes.Multiline = true;
            notes.ReadOnly = true;
            notes.ScrollBars = ScrollBars.Vertical;
            notes.Visible = false;
            page.Controls.Add(notes);

            askUpdates.Text = "Check for updates when Nebula Archipelago opens";
            askUpdates.Location = new Point(12, 440);
            askUpdates.Size = new Size(500, 26);
            askUpdates.Checked = session.AskAboutUpdates;
            page.Controls.Add(askUpdates);

            var info = new Label
            {
                Text = "Updates come from github.com/" + BuildInfo.Repository + "/releases. The installer is checked against its SHA-256 before it runs; your saves and settings are kept.",
                Location = new Point(12, 472), Size = new Size(700, 44), ForeColor = SystemColors.GrayText
            };
            page.Controls.Add(info);
            status.Text = "Not checked yet.";
        }

        /// <summary>Asks GitHub in the background. <paramref name="automatic"/>: at opening (no complaint if it fails).</summary>
        public void CheckAsync(bool automatic)
        {
            if (checking) return;
            checking = true;
            lastAutomatic = automatic;
            check.Enabled = false;
            status.Text = "Checking for updates…";
            ThreadPool.QueueUserWorkItem(delegate
            {
                ReleaseInfo release = null;
                string error = null;
                try { release = Updater.FindNewer(20000); }
                catch (Exception failure) { error = failure.Message; }
                try { page.BeginInvoke((Action)delegate { Done(release, error, automatic); }); }
                catch (InvalidOperationException) { }
            });
        }

        private void Done(ReleaseInfo release, string error, bool automatic)
        {
            checking = false;
            check.Enabled = true;
            if (error != null)
            {
                status.Text = automatic ? "Couldn't check for updates (no connection?)." : "Couldn't check for updates: " + error;
                return;
            }
            if (release == null)
            {
                found = null;
                install.Enabled = false;
                notes.Visible = false;
                status.Text = "Nebula Archipelago " + BuildInfo.Version + " is up to date.";
                return;
            }
            found = release;
            install.Enabled = true;
            install.Text = "Install " + release.Version;
            status.Text = "A new version is available: " + release.Version + ".";
            notes.Text = (release.Notes ?? "").Replace("\r\n", "\n").Replace("\n", "\r\n");
            notes.Visible = notes.Text.Length > 0;
            if (UpdateFound != null) UpdateFound(release);
        }

        private void InstallFound()
        {
            if (found == null) return;
            string setup;
            using (var download = new UpdateDownloadForm(found, new InstallLayout(session.Root).Staging))
            {
                if (download.ShowDialog(page.FindForm()) != DialogResult.OK) return;
                setup = download.SetupPath;
            }
            UpdateCheck.RunInstaller(setup);
            // The installer closes and replaces this program, then starts it again.
            Application.Exit();
        }
    }
}
