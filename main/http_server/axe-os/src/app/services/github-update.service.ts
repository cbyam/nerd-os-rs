import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, EMPTY } from 'rxjs';
import { map, switchMap, expand, scan, takeWhile, last } from 'rxjs/operators';

interface GithubAsset {
  id: number;
  name: string;
  browser_download_url: string;
  size: number;
}

export interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
  body: string;
  published_at: string;
  assets: GithubAsset[];
  isLatest?: boolean;
}

export interface VersionComparison {
  isNewer: boolean;
  isSame: boolean;
  isOlder: boolean;
  currentVersion: string;
  latestVersion: string;
}

export enum UpdateStatus {
  UP_TO_DATE = 'up-to-date',
  UPDATE_AVAILABLE = 'update-available',
  OUTDATED = 'outdated',
  UNKNOWN = 'unknown'
}

@Injectable({
  providedIn: 'root'
})
export class GithubUpdateService {

  private readonly baseReleasesUrl =
    'https://api.github.com/repos/shufps/nerd-os-rs/releases';

  constructor(
    private httpClient: HttpClient
  ) { }

  /** Fetch a single page of releases */
  private fetchReleasePage(page: number, perPage = 50): Observable<GithubRelease[]> {
    const url = `${this.baseReleasesUrl}?per_page=${perPage}&page=${page}`;
    return this.httpClient.get<GithubRelease[]>(url);
  }

  /**
   * Fetch releases of ONE type (stable or prerelease) until we have
   * at least `targetCount` items or there are no more pages.
   */
  private loadReleasesOfType(
    includePrereleases: boolean,
    targetCount = 10,
    maxPages = 10,
    perPage = 50
  ): Observable<GithubRelease[]> {
    const isPre = (r: GithubRelease) =>
      r.prerelease || this.isPrereleaseTag(r.tag_name);

    const isStable = (r: GithubRelease) => !isPre(r);

    const matchesType = includePrereleases ? isPre : isStable;

    // start with page 1
    return this.fetchReleasePage(1, perPage).pipe(
      expand((releases, index) => {
        const nextPage = index + 2; // index starts at 0 (page 1)
        const isLastPage = releases.length < perPage;
        const reachedMaxPages = nextPage > maxPages;

        if (isLastPage || reachedMaxPages) {
          return EMPTY;
        }

        return this.fetchReleasePage(nextPage, perPage);
      }),
      // accumulate only matching releases
      scan((acc, releases) => {
        const filtered = releases.filter(matchesType);
        return acc.concat(filtered);
      }, [] as GithubRelease[]),
      // solange weiter sammeln, bis wir genug haben
      takeWhile(acc => acc.length < targetCount, true),
      // am Ende letztes akkumuliertes Array liefern
      last(),
      // auf gewünschte Anzahl begrenzen
      map(acc => acc.slice(0, targetCount))
    );
  }

  /**
   * Fetch either:
   *  - up to 10 stable releases (includePrereleases = false)
   *  - up to 10 prereleases (includePrereleases = true)
   *
   * Es werden mehrere Seiten geladen, bis genug Releases vom gewünschten Typ
   * gefunden wurden oder keine Releases mehr da sind.
   */
  public getReleases(includePrereleases = false): Observable<GithubRelease[]> {
    const latest$ = this.httpClient.get<GithubRelease>(
      `${this.baseReleasesUrl}/latest`
    );

    const selected$ = this.loadReleasesOfType(includePrereleases, 10, 10, 50);

    return selected$.pipe(
      switchMap((releases: GithubRelease[]) =>
        latest$.pipe(
          map((latest) => {
            const list = releases.map(r => ({
              ...r,
              body: r.body || '',
              isLatest: !includePrereleases && r.id === latest.id
            }));
            // GitHub lists releases by creation date, so e.g. an LTS release made after
            // the latest one would come first. "latest" always goes on top (it is also
            // the default selection in the update dialog).
            const idx = list.findIndex(r => r.isLatest);
            if (idx > 0) {
              list.unshift(...list.splice(idx, 1));
            }
            return list;
          })
        )
      )
    );
  }

  /** A tag with a pre-release suffix, e.g. `v2.0.0-alpha.1` or `v2.0.0-rc.2`. */
  public isPrereleaseTag(tag: string): boolean {
    return tag.replace(/^v/, '').includes('-');
  }

  /**
   * Compare two versions like semantic versioning: `MAJOR.MINOR.PATCH`, then
   * a version with a pre-release suffix (`-alpha.1`, `-rc.2`) is older than
   * the same version without one; suffixes compare part by part (numbers
   * numerically, words alphabetically, fewer parts first).
   * Returns: 1 if v1 > v2, -1 if v1 < v2, 0 if equal
   */
  public compareVersions(v1: string, v2: string): number {
    const parse = (v: string) => {
      const clean = v.replace(/^v/, '');
      const dash = clean.indexOf('-');
      const core = dash < 0 ? clean : clean.slice(0, dash);
      const pre = dash < 0 ? [] : clean.slice(dash + 1).split('.');
      return { core: core.split('.').map(p => parseInt(p) || 0), pre };
    };
    const a = parse(v1);
    const b = parse(v2);

    for (let i = 0; i < Math.max(a.core.length, b.core.length); i++) {
      const p1 = a.core[i] || 0;
      const p2 = b.core[i] || 0;
      if (p1 !== p2) return p1 > p2 ? 1 : -1;
    }

    // same core: a release is newer than its pre-releases
    if (!a.pre.length || !b.pre.length) {
      return a.pre.length === b.pre.length ? 0 : (a.pre.length ? -1 : 1);
    }
    for (let i = 0; i < Math.max(a.pre.length, b.pre.length); i++) {
      const p1 = a.pre[i];
      const p2 = b.pre[i];
      if (p1 === undefined) return -1;
      if (p2 === undefined) return 1;
      const n1 = /^\d+$/.test(p1);
      const n2 = /^\d+$/.test(p2);
      if (n1 && n2) {
        const d = parseInt(p1) - parseInt(p2);
        if (d !== 0) return d > 0 ? 1 : -1;
      } else if (n1 !== n2) {
        return n1 ? -1 : 1;
      } else if (p1 !== p2) {
        return p1 > p2 ? 1 : -1;
      }
    }
    return 0;
  }

  /**
   * Compare current version with latest release
   */
  public getVersionComparison(currentVersion: string, latestRelease: GithubRelease): VersionComparison {
    const comparison = this.compareVersions(currentVersion, latestRelease.tag_name);

    return {
      isNewer: comparison > 0,
      isSame: comparison === 0,
      isOlder: comparison < 0,
      currentVersion,
      latestVersion: latestRelease.tag_name
    };
  }

  /**
   * Get update status based on version comparison
   */
  public getUpdateStatus(currentVersion: string, latestRelease: GithubRelease | null): UpdateStatus {
    if (!latestRelease) {
      return UpdateStatus.UNKNOWN;
    }

    const comparison = this.compareVersions(currentVersion, latestRelease.tag_name);

    if (comparison === 0) {
      return UpdateStatus.UP_TO_DATE;
    } else if (comparison < 0) {
      return UpdateStatus.UPDATE_AVAILABLE;
    } else {
      return UpdateStatus.OUTDATED;
    }
  }

  /**
   * Download firmware directly from GitHub
   */
  public downloadFirmware(url: string): Observable<any> {
    return this.httpClient.get(url, {
      responseType: 'blob',
      reportProgress: true,
      observe: 'events'
    });
  }

  /**
   * Get changelog formatted as HTML
   */
  public getChangelog(release: GithubRelease): string {
    if (!release.body) {
      return 'No changelog available';
    }

    // Convert markdown to basic HTML (simple conversion)
    return release.body
      .replace(/### (.*)/g, '<h4>$1</h4>')
      .replace(/## (.*)/g, '<h3>$1</h3>')
      .replace(/# (.*)/g, '<h2>$1</h2>')
      .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
      .replace(/\*(.*?)\*/g, '<em>$1</em>')
      .replace(/\n/g, '<br>');
  }

  /**
   * Find asset in release by filename
   */
  public findAsset(release: GithubRelease, filename: string): GithubAsset | undefined {
    return release.assets.find(asset => asset.name === filename);
  }


}
