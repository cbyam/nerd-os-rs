import { Component, EventEmitter, OnInit, Output } from '@angular/core';
import { HttpErrorResponse } from '@angular/common/http';
import { NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { switchMap } from 'rxjs/operators';
import { SystemService } from '../../services/system.service';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { IProfile, IProfiles, IProfileSettings } from '../../models/IProfiles';
import { ISettingsV2 } from '../../models/ISettingsV2';

/**
 * One row above the settings: pick a profile and apply it, save the
 * current settings as a new profile, delete one.
 */
@Component({
  selector: 'app-profiles',
  templateUrl: './profiles.component.html',
  styleUrls: ['./profiles.component.scss'],
})
export class ProfilesComponent implements OnInit {
  /** a profile was applied: the settings changed */
  @Output() applied = new EventEmitter<number>();

  public profiles: IProfile[] = [];
  public max = 5;
  public active: number | null = null;
  public selected: number | null = null;

  /** the name input of "+" is open */
  public adding = false;
  public newName = '';

  constructor(
    private system: SystemService,
    private otpAuth: OtpAuthService,
    private toastr: NbToastrService,
    private translate: TranslateService,
  ) { }

  ngOnInit(): void {
    this.system.getProfiles().subscribe({ next: p => this.set(p), error: () => { } });
  }

  private set(p: IProfiles): void {
    this.profiles = p.profiles ?? [];
    this.max = p.max ?? 5;
    this.active = p.active ?? null;
    if (this.selected === null || this.selected >= this.profiles.length) {
      this.selected = this.active ?? (this.profiles.length ? 0 : null);
    }
  }

  public get canAdd(): boolean {
    return this.profiles.length < this.max;
  }

  /** The profile settings of the device's saved settings. */
  private fromSettings(c: ISettingsV2): IProfileSettings {
    return {
      frequency: c.frequency,
      coreVoltage: c.coreVoltage,
      poolMode: c.poolMode,
      ...(c.poolMode === 1 ? { poolBalance: c.poolBalance || 50 } : {}),
      fans: (c.fans ?? []).map(f => ({
        mode: f.mode, manualSpeed: f.manualSpeed, overheatTemp: f.overheatTemp,
        pid: { targetTemp: f.pid.targetTemp, p: f.pid.p, i: f.pid.i, d: f.pid.d },
      })),
    };
  }

  public startAdd(): void {
    this.newName = '';
    this.adding = true;
  }

  /** "+": the current (saved) settings as a new profile. */
  public add(): void {
    const name = this.newName.trim();
    if (!name) return;
    this.system.getSettingsV2().pipe(
      switchMap(c => {
        const list = [...this.profiles, { name, settings: this.fromSettings(c) }];
        return this.store$(list);
      }),
    ).subscribe({
      next: p => {
        this.set(p);
        this.selected = this.profiles.length - 1;
        this.adding = false;
        this.toastr.success(this.translate.instant('PROFILES.SAVED'), this.translate.instant('COMMON.SUCCESS'));
      },
      error: err => this.failed(err),
    });
  }

  public remove(): void {
    if (this.selected === null) return;
    const i = this.selected;
    const list = this.profiles.filter((_, j) => j !== i);
    this.store$(list).subscribe({
      next: p => {
        this.selected = null;
        this.set(p);
        this.toastr.success(this.translate.instant('PROFILES.DELETED'), this.translate.instant('COMMON.SUCCESS'));
      },
      error: err => this.failed(err),
    });
  }

  private store$(list: IProfile[]) {
    return this.otpAuth.ensureOtp$(
      '',
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT'),
    ).pipe(
      switchMap(({ totp }: EnsureOtpResult) => this.system.updateProfiles(list, totp)),
    );
  }

  private failed(err: HttpErrorResponse): void {
    const msg = typeof err?.error === 'string' && err.error ? err.error : this.translate.instant('PROFILES.SAVE_FAILED');
    this.toastr.danger(msg, this.translate.instant('COMMON.ERROR'));
  }

  public apply(): void {
    if (this.selected === null) return;
    this.system.applyProfile(this.selected).subscribe({
      next: r => {
        this.active = r.id;
        this.applied.emit(r.id);
        this.toastr.success(
          this.translate.instant('PROFILES.APPLIED', { name: r.name }),
          this.translate.instant('COMMON.SUCCESS'));
      },
      error: () => {
        this.toastr.danger(this.translate.instant('PROFILES.APPLY_FAILED'), this.translate.instant('COMMON.ERROR'));
      },
    });
  }
}
