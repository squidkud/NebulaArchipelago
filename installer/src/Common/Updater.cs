using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;

namespace Nebula
{
    /// <summary>A published release that is newer than the running version.</summary>
    internal sealed class ReleaseInfo
    {
        public string Version, Tag, Notes, SetupUrl, HashUrl;
        public long SetupSize;
        public bool Prerelease;
    }

    /// <summary>
    /// Update discovery through the GitHub releases of the Nebula Archipelago repository (BuildInfo.Repository).
    /// A release holds the installer (NebulaArchipelago-Setup.exe) and its SHA-256 (NebulaArchipelago-Setup.exe.sha256);
    /// the launcher downloads both, checks the installer against the hash and starts it. The installer replaces the
    /// installed version in place (it is an ordinary Inno Setup installer; nothing copies or deletes itself).
    /// </summary>
    internal static class Updater
    {
        /// <summary>The releases list (tests point it at a local server).</summary>
        internal static string ReleasesUrl = "https://api.github.com/repos/" + BuildInfo.Repository + "/releases?per_page=30";

        public const string SetupAsset = "NebulaArchipelago-Setup.exe";
        public const string BetaSetupAsset = "NebulaArchipelago-Beta-Setup.exe";

        public static ReleaseInfo FindNewer(int timeoutMilliseconds = 30000)
        {
            string json = Downloader.GetString(ReleasesUrl, "application/vnd.github+json", timeoutMilliseconds);
            return SelectNewer(json, BuildInfo.Version);
        }

        // A beta updates to betas only, a release to releases only.
        internal static ReleaseInfo SelectNewer(string json, string installedVersion)
        {
            bool beta = IsBetaVersion(installedVersion);
            string setupName = beta ? BetaSetupAsset : SetupAsset;
            ReleaseInfo best = null;
            foreach (var item in (System.Collections.IList)Json.ParseAny(json))
            {
                var release = (Dictionary<string, object>)item;
                if (Json.Bool(release, "draft")) continue;
                string tag = Json.Str(release, "tag_name") ?? "";
                string version = tag.StartsWith("v") ? tag.Substring(1) : tag;
                if (IsBetaVersion(version) != beta) continue;
                if (CompareVersions(version, best != null ? best.Version : installedVersion) <= 0) continue;
                string setup = null, hash = null;
                long setupSize = 0;
                foreach (var assetItem in Json.List(release, "assets") ?? new object[0])
                {
                    var asset = (Dictionary<string, object>)assetItem;
                    string name = Json.Str(asset, "name");
                    if (name == setupName)
                    {
                        setup = Json.Str(asset, "browser_download_url");
                        setupSize = Json.Long(asset, "size");
                    }
                    if (name == setupName + ".sha256") hash = Json.Str(asset, "browser_download_url");
                }
                if (setup == null || hash == null) continue;
                best = new ReleaseInfo
                {
                    Version = version, Tag = tag, Notes = Json.Str(release, "body") ?? "",
                    SetupUrl = setup, HashUrl = hash, SetupSize = setupSize,
                    Prerelease = Json.Bool(release, "prerelease")
                };
            }
            return best;
        }

        internal static bool IsBetaVersion(string version)
        {
            return version != null && version.IndexOf("-beta.", StringComparison.OrdinalIgnoreCase) >= 0;
        }

        /// <summary>Download a release's installer and check it against the release's SHA-256; returns its path.</summary>
        public static string Download(ReleaseInfo release, string directory, Action<long> onBytes, System.Threading.CancellationToken cancel)
        {
            string folder = Path.Combine(directory, "update-" + release.Version);
            string exe = Path.Combine(folder, IsBetaVersion(release.Version) ? BetaSetupAsset : SetupAsset);
            string hash = exe + ".sha256";
            // The hash is fetched afresh each time; a download that fails the check is made once more from the start.
            for (int attempt = 0; ; attempt++)
            {
                if (File.Exists(exe)) File.Delete(exe);
                if (File.Exists(hash)) File.Delete(hash);
                if (attempt > 0 && File.Exists(exe + ".partial")) File.Delete(exe + ".partial");
                Downloader.Fetch(release.HashUrl, hash, null, cancel);
                Downloader.Fetch(release.SetupUrl, exe, onBytes, cancel);
                if (VerifyHash(exe, hash)) return exe;
                File.Delete(exe);
                if (attempt == 1)
                    throw new InvalidDataException("The downloaded Nebula Archipelago " + release.Version + " installer does not match its SHA-256. It was deleted and not run.");
            }
        }

        /// <summary>The file's SHA-256 equals the first word of the hash file ("hex  name" or just "hex").</summary>
        public static bool VerifyHash(string file, string hashFile)
        {
            string expected;
            try
            {
                string text = File.ReadAllText(hashFile).Trim();
                int space = text.IndexOfAny(new[] { ' ', '\t', '\r', '\n', '*' });
                expected = (space < 0 ? text : text.Substring(0, space)).ToLowerInvariant();
            }
            catch (IOException) { return false; }
            if (expected.Length != 64) return false;
            using (var stream = File.OpenRead(file))
            using (var sha = SHA256.Create())
            {
                var bytes = sha.ComputeHash(stream);
                var actual = new System.Text.StringBuilder(64);
                foreach (byte b in bytes) actual.Append(b.ToString("x2"));
                return actual.ToString() == expected;
            }
        }

        /// <summary>Compare MAJOR.MINOR.PATCH[-preview.N]; a release sorts after its previews.</summary>
        public static int CompareVersions(string a, string b)
        {
            long[] x = Key(a), y = Key(b);
            for (int i = 0; i < x.Length; i++) if (x[i] != y[i]) return x[i].CompareTo(y[i]);
            return 0;
        }

        private static long[] Key(string version)
        {
            var key = new long[5];
            if (string.IsNullOrEmpty(version)) return key;
            string[] parts = version.Split(new[] { '-' }, 2);
            string[] numbers = parts[0].Split('.');
            for (int i = 0; i < 3 && i < numbers.Length; i++) long.TryParse(numbers[i], out key[i]);
            key[3] = parts.Length == 1 ? 1 : 0;
            if (parts.Length == 2)
            {
                int dot = parts[1].LastIndexOf('.');
                if (dot >= 0) long.TryParse(parts[1].Substring(dot + 1), out key[4]);
            }
            return key;
        }
    }
}
