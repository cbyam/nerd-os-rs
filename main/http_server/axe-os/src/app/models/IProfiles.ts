import { ISettingsV2Fan } from './ISettingsV2';

/** The settings a profile may hold (a subset of PATCH /api/v2/settings). */
export interface IProfileSettings {
    frequency?: number;
    coreVoltage?: number;
    poolMode?: number;
    poolBalance?: number;
    fans?: Partial<ISettingsV2Fan>[];
}

export interface IProfile {
    id?: number;
    name: string;
    settings: IProfileSettings;
}

export interface IProfiles {
    max: number;
    active: number | null;
    profiles: IProfile[];
}
