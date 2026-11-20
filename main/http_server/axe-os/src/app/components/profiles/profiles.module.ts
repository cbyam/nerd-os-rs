import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { NbButtonModule, NbIconModule, NbInputModule, NbSelectModule, NbTooltipModule } from '@nebular/theme';
import { TranslateModule } from '@ngx-translate/core';
import { ProfilesComponent } from './profiles.component';

@NgModule({
  declarations: [ProfilesComponent],
  imports: [
    CommonModule,
    FormsModule,
    NbButtonModule,
    NbIconModule,
    NbInputModule,
    NbSelectModule,
    NbTooltipModule,
    TranslateModule,
  ],
  exports: [ProfilesComponent],
})
export class ProfilesModule { }
