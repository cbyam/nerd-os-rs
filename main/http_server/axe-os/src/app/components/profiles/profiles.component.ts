import { Component, OnInit } from '@angular/core';
import { HttpErrorResponse } from '@angular/common/http';
import { NbToastrService } from '@nebular/theme';
import { TranslateService } from '@ngx-translate/core';
import { forkJoin, of } from 'rxjs';
import { catchError, switchMap } from 'rxjs/operators';
import { SystemService } from '../../services/system.service';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { IProfile, IProfiles, IProfileSettings } from '../../models/IProfiles';
import { ISettingsV2 } from '../../models/ISettingsV2';

/** The editor's form: which settings the profile holds, and their values. */
interface ProfileForm {
  name: string;
  useAsic: boolean;
  frequency: number | null;
  coreVoltage: number | null;
  usePool: boolean;
  poolMode: number;
  poolBalance: number;
  useFans: boolean;
}

@Component({
  selector: 'app-profiles',
  templateUrl: './profiles.component.html',
  styleUrls: ['./profiles.component.scss'],
})
export class ProfilesComponent implements OnInit {
  public profiles: IProfile[] = [];
  public max = 5;
  public active: number | null = null;
  public loaded = false;

  /** index being edited, profiles.length: a new one, null: none */
  public editing: number | null = null;
  public form: ProfileForm = this.emptyForm();

  private current: ISettingsV2 | null = null;
  /** the fans of the profile being edited (kept unless replaced) */
  private editFans: IProfileSettings['fans'];

  constructor(
    private system: SystemService,
    private otpAuth: OtpAuthService,
    private toastr: NbToastrService,
    private translate: TranslateService,
  ) { }

  ngOnInit(): void {
    this.load();
  }

  private load(): void {
    forkJoin({
      profiles: this.system.getProfiles(),
      settings: this.system.getSettingsV2().pipe(catchError(() => of(null))),
    }).subscribe({
      next: ({ profiles, settings }) => {
        this.set(profiles);
        this.current = settings;
        this.loaded = true;
      },
      error: () => { this.loaded = true; },
    });
  }

  private set(p: IProfiles): void {
    this.profiles = p.profiles ?? [];
    this.max = p.max ?? 5;
    this.active = p.active ?? null;
  }

  private emptyForm(): ProfileForm {
    return {
      name: '', useAsic: false, frequency: null, coreVoltage: null,
      usePool: false, poolMode: 0, poolBalance: 50, useFans: false,
    };
  }

  /** A short description of what a profile sets. */
  public summary(p: IProfile): string {
    const s = p.settings;
    const parts: string[] = [];
    if (s.frequency) parts.push(`${s.frequency} MHz`);
    if (s.coreVoltage) parts.push(`${s.coreVoltage} mV`);
    if (s.poolMode === 0) parts.push(this.translate.instant('PROFILES.FAILOVER'));
    if (s.poolMode === 1) parts.push(this.translate.instant('PROFILES.DUAL'));
    if (s.poolBalance) parts.push(`${s.poolBalance}/${100 - s.poolBalance}`);
    if (s.fans?.length) parts.push(this.translate.instant('PROFILES.FANS'));
    return parts.join(' · ');
  }

  public get canAdd(): boolean {
    return this.editing === null && this.profiles.length < this.max;
  }

  public add(): void {
    this.editing = this.profiles.length;
    this.form = this.emptyForm();
    this.editFans = undefined;
    this.fillFromCurrent();
  }

  public edit(i: number): void {
    const s = this.profiles[i].settings;
    this.editing = i;
    this.editFans = s.fans;
    this.form = {
      name: this.profiles[i].name,
      useAsic: s.frequency !== undefined || s.coreVoltage !== undefined,
      frequency: s.frequency ?? null,
      coreVoltage: s.coreVoltage ?? null,
      usePool: s.poolMode !== undefined || s.poolBalance !== undefined,
      poolMode: s.poolMode ?? 0,
      poolBalance: s.poolBalance ?? 50,
      useFans: !!s.fans?.length,
    };
  }

  /** Takes the device's current values into the form. */
  public fillFromCurrent(): void {
    const c = this.current;
    if (!c) return;
    this.form.frequency = c.frequency;
    this.form.coreVoltage = c.coreVoltage;
    this.form.poolMode = c.poolMode;
    this.form.poolBalance = c.poolBalance || 50;
    this.editFans = this.currentFans();
  }

  private currentFans(): IProfileSettings['fans'] {
    return (this.current?.fans ?? []).map(f => ({
      mode: f.mode, manualSpeed: f.manualSpeed, overheatTemp: f.overheatTemp,
      pid: { targetTemp: f.pid.targetTemp, p: f.pid.p, i: f.pid.i, d: f.pid.d },
    }));
  }

  public cancel(): void {
    this.editing = null;
  }

  private settingsFromForm(): IProfileSettings {
    const f = this.form;
    const s: IProfileSettings = {};
    if (f.useAsic) {
      if (f.frequency) s.frequency = Number(f.frequency);
      if (f.coreVoltage) s.coreVoltage = Number(f.coreVoltage);
    }
    if (f.usePool) {
      s.poolMode = Number(f.poolMode);
      if (s.poolMode === 1) s.poolBalance = Number(f.poolBalance);
    }
    if (f.useFans) {
      s.fans = this.editFans?.length ? this.editFans : this.currentFans();
    }
    return s;
  }

  public save(): void {
    if (this.editing === null || !this.form.name.trim()) return;
    const list = this.profiles.map(p => ({ name: p.name, settings: p.settings }));
    const entry = { name: this.form.name.trim(), settings: this.settingsFromForm() };
    if (this.editing < list.length) {
      list[this.editing] = entry;
    } else {
      list.push(entry);
    }
    this.store(list, () => { this.editing = null; });
  }

  public remove(i: number): void {
    const list = this.profiles.filter((_, j) => j !== i);
    this.store(list);
  }

  private store(list: IProfile[], done?: () => void): void {
    this.otpAuth.ensureOtp$(
      '',
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT'),
    ).pipe(
      switchMap(({ totp }: EnsureOtpResult) => this.system.updateProfiles(list, totp)),
    ).subscribe({
      next: (p) => {
        this.set(p);
        done?.();
        this.toastr.success(this.translate.instant('PROFILES.SAVED'), this.translate.instant('COMMON.SUCCESS'));
      },
      error: (err: HttpErrorResponse) => {
        const msg = typeof err.error === 'string' && err.error ? err.error : this.translate.instant('PROFILES.SAVE_FAILED');
        this.toastr.danger(msg, this.translate.instant('COMMON.ERROR'));
      },
    });
  }

  public apply(i: number): void {
    this.system.applyProfile(i).subscribe({
      next: (r) => {
        this.active = r.id;
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
