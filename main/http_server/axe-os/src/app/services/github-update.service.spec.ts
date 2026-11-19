import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideHttpClientTesting } from '@angular/common/http/testing';

import { GithubUpdateService } from './github-update.service';

describe('GithubUpdateService', () => {
  let service: GithubUpdateService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()],
    });
    service = TestBed.inject(GithubUpdateService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('orders versions with pre-releases before the release', () => {
    const ordered = [
      'v1.1.2',
      'v2.0.0-alpha.1',
      'v2.0.0-alpha.2',
      'v2.0.0-alpha.10',
      'v2.0.0-beta.1',
      'v2.0.0-rc.1',
      'v2.0.0',
      'v2.0.1',
      'v2.1.0',
    ];
    for (let i = 0; i < ordered.length; i++) {
      for (let j = 0; j < ordered.length; j++) {
        const expected = i === j ? 0 : (i > j ? 1 : -1);
        expect(service.compareVersions(ordered[i], ordered[j]))
          .withContext(`${ordered[i]} vs ${ordered[j]}`)
          .toBe(expected);
      }
    }
  });

  it('ignores the v prefix', () => {
    expect(service.compareVersions('2.0.0-alpha.1', 'v2.0.0-alpha.1')).toBe(0);
  });

  it('recognizes pre-release tags', () => {
    expect(service.isPrereleaseTag('v2.0.0-alpha.1')).toBeTrue();
    expect(service.isPrereleaseTag('v2.0.0-rc.1')).toBeTrue();
    expect(service.isPrereleaseTag('v2.0.0')).toBeFalse();
  });
});
