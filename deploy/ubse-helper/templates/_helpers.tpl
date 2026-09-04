{{/*
Create chart name and version as used by the chart label.
*/}}
{{- define "ubse-helper.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Common labels
*/}}
{{- define "ubse-helper.labels" -}}
helm.sh/chart: {{ include "ubse-helper.chart" . }}
{{ include "ubse-helper.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{/*
Selector labels
*/}}
{{- define "ubse-helper.selectorLabels" -}}
app.kubernetes.io/name: {{ include "ubse-helper.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{/*
Expand the name of the chart.
*/}}
{{- define "ubse-helper.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Create a default fully qualified app name.
fullnameOverride 强制为 "ubse-helper",保证 Service/Endpoints/DaemonSet 同名(AC-CTN-DEPLOY-01)。
*/}}
{{- define "ubse-helper.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := default .Chart.Name .Values.nameOverride }}
{{- if contains $name .Release.Name }}
{{- .Release.Name | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}
{{- end }}

{{/*
ServiceAccount name
*/}}
{{- define "ubse-helper.serviceAccountName" -}}
{{- if .Values.serviceAccount.create }}
{{- default (include "ubse-helper.fullname" .) .Values.serviceAccount.name }}
{{- else }}
{{- default "default" .Values.serviceAccount.name }}
{{- end }}
{{- end }}